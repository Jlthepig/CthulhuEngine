// the host app picks its OpenGL loader; it must be loaded before init()
#ifdef OCTOGUI_GL_LOADER_HEADER
#include OCTOGUI_GL_LOADER_HEADER
#else
#include "glad/gl.h"
#endif

#include "OctoGui_OpenGL.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace octogui
{
    namespace {
        constexpr f32 Pi = 3.14159265358979323846f; 

        constexpr int roundedSegmentsPerCorner = 12;
        constexpr int maxRoundedPoints = (roundedSegmentsPerCorner + 1) * 4;

        constexpr int shadowSegmentsPerCorner = 6;
        constexpr int maxShadowPoints = (shadowSegmentsPerCorner + 1) * 4;

        const char* vertexShaderSource = R"(#version 330 core
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec4 color;
        layout(location = 2) in vec2 uv;

        uniform mat4 projection;

        out vec4 vertexColor;
        out vec2 vertexUV;

        void main() {
            vertexColor = color;
            vertexUV = uv;
            gl_Position = projection * vec4(position, 0.0, 1.0);
        }
        )";

        const char* fragmentShaderSource = R"(#version 330 core
        in vec4 vertexColor;
        in vec2 vertexUV;

        uniform sampler2D uTexture;

        out vec4 fragmentColor;

        void main() {
            fragmentColor = vertexColor * texture(uTexture,vertexUV);
        }
        )";

        bool compileShader(GLenum type, const char* source, GLuint& outShader) {
            GLuint shader = glCreateShader(type);

            glShaderSource(shader,1, &source, nullptr);
            glCompileShader(shader);

            GLint success = 0;

            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (!success) {
                GLchar infoLog[1024];
                glGetShaderInfoLog(shader,1024,nullptr,infoLog);
                std::fprintf(stderr, "OctoGUI OpenGL shader compile error:\n%s\n",infoLog);
                glDeleteShader(shader);
                return false;
            }

            outShader = shader;

            return true;
        }

        bool linkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint& outProgram) {
            GLuint program = glCreateProgram();

            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glLinkProgram(program);

            GLint success = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &success);

            if (!success) {
                GLchar infoLog[1024];
                glGetProgramInfoLog(program, 1024, nullptr, infoLog);
                std::fprintf(stderr, "OctoGUI OpenGL program link error:\n%s\n", infoLog);
                glDeleteProgram(program);
                return false;
            }

            outProgram = program;
            return true;
        }

        void makeOrthographic(f32 width, f32 height, f32* matrix) {
            for (int i = 0; i < 16; ++i) {
                matrix[i] = 0.0f;
            }

            if (width <= 0.0f || height <= 0.0f) {
                matrix[0] = 1.0f;
                matrix[5] = 1.0f;
                matrix[10] = 1.0f;
                matrix[15] = 1.0f;
                return;
            }

            matrix[0] = 2.0f / width;
            matrix[5] = -2.0f / height;
            matrix[10] = -1.0f;
            matrix[12] = -1.0f;
            matrix[13] = 1.0f;
            matrix[15] = 1.0f;
        }

        f32 clampRadius(Rect rect, f32 radius) noexcept {
            if (radius <= 0.0f) {
                return 0.0f;
            }

            const f32 smallerSide = rect.w < rect.h ? rect.w : rect.h;
            const f32 maxRadius = smallerSide * 0.5f;

            if (maxRadius <= 0.0f) {
                return 0.0f;
            }

            return radius > maxRadius ? maxRadius : radius;
        }

        bool sameTexture(TextureHandle a, TextureHandle b) noexcept {
            return a.index == b.index && a.generation == b.generation;
        }

        

        usize buildRoundedRectPerimeter(Rect rect, f32 radius, Vec2* points, usize maxPoints, int segmentsPerCorner = roundedSegmentsPerCorner) {
            if (rect.isEmpty() || !points || maxPoints == 0) {return 0;}

            if (segmentsPerCorner <= 0) {return 0;}

            radius = clampRadius(rect, radius);

            const f32 left = rect.x;
            const f32 top = rect.y;
            const f32 right = rect.x + rect.w;
            const f32 bottom = rect.y + rect.h;
            
            usize count = 0;

            auto addPoint = [&](Vec2 p) {
                if (count < maxPoints) {
                    points[count] = p;
                    ++count;
                }
            };

            auto addArc = [&](f32 centerX, f32 centerY, f32 startAngle, f32 endAngle) {
                for (int i = 0; i <= segmentsPerCorner; ++i) {
                    const f32 t = static_cast<f32>(i) / static_cast<f32>(segmentsPerCorner);
                    const f32 angle = startAngle + (endAngle - startAngle) * t;

                   addPoint( Vec2{ centerX + radius * std::cos(angle), centerY + radius * std::sin(angle) });
                }
            };

            addArc(left + radius, top + radius, Pi, Pi * 1.5f);
            addArc(right - radius, top + radius, Pi * 1.5f, Pi * 2.0f);
            addArc(right - radius, bottom - radius, 0.0f, Pi * 0.5f);
            addArc(left + radius, bottom - radius, Pi * 0.5f, Pi);

            return count < maxPoints ? count : maxPoints;
        }

        Color gradientColorAt(Vec2 position, Rect rect, const LinearGradient& gradient) {
            Vec2 direction = gradient.direction;

            const f32 lengthSquared = direction.x * direction.x + direction.y * direction.y;

            if (lengthSquared <= 0.000001f) {
                direction = Vec2{0.0f, 1.0f};
            } else {
                const f32 inverseLength = 1.0f / std::sqrt(lengthSquared);
                direction.x *= inverseLength;
                direction.y *= inverseLength;
            }

            const Vec2 center = rect.center();
            const f32 halfWidth = rect.w * 0.5f;
            const f32 halfHeight = rect.h * 0.5f;
            const f32 extent = std::abs(direction.x) * halfWidth + std::abs(direction.y) * halfHeight;

            if (extent <= 0.000001f) {return gradient.start;}

            const Vec2 offset{position.x - center.x, position.y - center.y};
            f32 t = 0.5f + (offset.x * direction.x + offset.y * direction.y) / (extent * 2.0f);

            if (t < 0.0f) {t = 0.0f;}
            if (t > 1.0f) {t = 1.0f;}

            return Color{
                gradient.start.r + (gradient.end.r - gradient.start.r) * t,
                gradient.start.g + (gradient.end.g - gradient.start.g) * t,
                gradient.start.b + (gradient.end.b - gradient.start.b) * t,
                gradient.start.a + (gradient.end.a - gradient.start.a) * t
            };
        }
    }

        bool OpenGLBackend::init() {
            GLuint vertexShader = 0;
            GLuint fragmentShader = 0;
            GLuint shaderProgram = 0;

            if (!compileShader(GL_VERTEX_SHADER, vertexShaderSource, vertexShader)) {
                return false;
            }

            if (!compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, fragmentShader)) {
                glDeleteShader(vertexShader);
                return false;
            }

            if (!linkProgram(vertexShader, fragmentShader, shaderProgram)) {
                glDeleteShader(vertexShader);
                glDeleteShader(fragmentShader);
                return false;
            }

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            GLuint vaoID = 0;
            GLuint vboID = 0;

            glGenVertexArrays(1, &vaoID);
            glGenBuffers(1, &vboID);

            glBindVertexArray(vaoID);
            glBindBuffer(GL_ARRAY_BUFFER, vboID);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(
                0,
                2,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(Vertex)),
                nullptr
            );

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(
                1,
                4,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(Vertex)),
                reinterpret_cast<const void*>(offsetof(Vertex, r))
            );

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(
                2,
                2,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(Vertex)),
                reinterpret_cast<const void*>(offsetof(Vertex, u))
            );

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            program = static_cast<unsigned int>(shaderProgram);
            vao = static_cast<unsigned int>(vaoID);
            vbo = static_cast<unsigned int>(vboID);

            projectionLocation = glGetUniformLocation(shaderProgram, "projection");
            textureLocation = glGetUniformLocation(shaderProgram, "uTexture");

            GLuint whiteID = 0;
            glGenTextures(1,&whiteID);
            glBindTexture(GL_TEXTURE_2D,whiteID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            const unsigned char whitePixels[4] = {255,255,255,255};

            glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixels);

            whiteTexture = static_cast<unsigned int>(whiteID);

            batchVertices.reserve(4096);

            return true;
    }

        void OpenGLBackend::shutdown() {

            for (TextureSlot& slot : textureSlots) {
                if (slot.id != 0) {
                    GLuint id = static_cast<GLuint>(slot.id);
                    glDeleteTextures(1,&id);
                    slot.id = 0;
                }
            }

            textureSlots.clear();
            freeTextureSlots.clear();

            if (whiteTexture != 0) {
                GLuint id = static_cast<GLuint>(whiteTexture);
                glDeleteTextures(1, &id);
                whiteTexture = 0;
            }

            if (vbo != 0) {
                GLuint id = static_cast<GLuint>(vbo);
                glDeleteBuffers(1, &id);
                vbo = 0;
            }

            if (vao != 0) {
                GLuint id = static_cast<GLuint>(vao);
                glDeleteVertexArrays(1, &id);
                vao = 0;
            }

            if (program != 0) {
                GLuint id = static_cast<GLuint>(program);
                glDeleteProgram(id);
                program = 0;
            }

            projectionLocation = -1;
            textureLocation = -1;

            batchVertices.clear();
            currentBatchTexture = TextureHandle{};
        }

        TextureHandle OpenGLBackend::createTexture(u32 width, u32 height, const void* pixels, TextureFormat format) {
            if (vao == 0 || width == 0 || height == 0) {return TextureHandle{};}

            u32 index;

            if (!freeTextureSlots.empty()) {
                index = freeTextureSlots.back();
                freeTextureSlots.pop_back();
            } else {
                index = static_cast<u32>(textureSlots.size());
                textureSlots.push_back(TextureSlot{});
            }

            TextureSlot& slot = textureSlots[index];

            if (slot.id == 0) {
                GLuint id = 0;
                glGenTextures(1, &id);
                slot.id = static_cast<unsigned int>(id);
            }

            slot.alive = true;

            uploadTexture(slot, width, height, pixels, format);

            return TextureHandle{index, slot.generation};
        }

        void OpenGLBackend::updateTexture(TextureHandle handle, u32 width, u32 height, const void* pixels, TextureFormat format) {
            if (!isValidTexture(handle) || width == 0 || height == 0 || !pixels) {return;}

            TextureSlot& slot = textureSlots[handle.index];

            if (slot.id == 0) {return;}

            if (slot.width == width && slot.height == height && slot.format == format) {
                GLuint id = static_cast<GLuint>(slot.id);
                glBindTexture(GL_TEXTURE_2D, id);

                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                if (format == TextureFormat::RGBA8) {
                    glTexSubImage2D(GL_TEXTURE_2D, 0,0,0, static_cast<GLsizei>(width), 
                    static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);

                } else  {
                    glTexSubImage2D(GL_TEXTURE_2D, 0,0,0, static_cast<GLsizei>(width), 
                    static_cast<GLsizei>(height), GL_RED, GL_UNSIGNED_BYTE, pixels);
                }
            } else {
                uploadTexture(slot, width, height, pixels, format);
            }
        }

        void OpenGLBackend::updateTextureRegion(TextureHandle handle, u32 x, u32 y, u32 width, u32 height, const void* pixels, u32 rowStrideBytes, TextureFormat format) {

            if (!isValidTexture(handle) || !pixels || width == 0 || height == 0) {return;}

            TextureSlot& slot = textureSlots[handle.index];

            if (x + width > slot.width || y + height > slot.height) {return;}

            if (slot.format != format) { return;}

            GLenum glFormat = GL_RGBA;
            u32 bytesPerPixel = 4;

            if (format == TextureFormat::R8) {glFormat = GL_RED; bytesPerPixel = 1;}

            if (rowStrideBytes < width * bytesPerPixel) {return;}

            glBindTexture(GL_TEXTURE_2D, slot.id);

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei( GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rowStrideBytes / bytesPerPixel));

            glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y), 
            static_cast<GLsizei>(width), static_cast<GLsizei>(height), glFormat, GL_UNSIGNED_BYTE, pixels);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void OpenGLBackend::destroyTexture(TextureHandle handle) {
            if (!isValidTexture(handle)) {return;}

            TextureSlot& slot = textureSlots[handle.index];

            if (slot.id != 0) {
                GLuint id = static_cast<GLuint>(slot.id);
                glDeleteTextures(1, &id);
                slot.id = 0;
            }

            slot.alive = false;
            slot.width = 0;
            slot.height = 0;

            ++slot.generation;

            if (slot.generation == 0) { slot.generation = 1;}

            freeTextureSlots.push_back(handle.index);
        }

        void OpenGLBackend::render(
            const DrawList& drawList,
            Vec2 displaySize,
            Vec2 framebufferSize
        ) {
            if (program == 0) {
                return;
            }

            displaySizeData = displaySize;
            framebufferSizeData = framebufferSize;

            clipStack.clear();
            batchVertices.clear();
            currentBatchTexture = TextureHandle{};

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glViewport(0,0,static_cast<GLsizei>(framebufferSize.x),static_cast<GLsizei>(framebufferSize.y));

            glUseProgram(static_cast<GLuint>(program));
            glBindVertexArray(static_cast<GLuint>(vao));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo));

            glActiveTexture(GL_TEXTURE0);

            if (textureLocation >= 0) {glUniform1i(textureLocation,0);}

            bindWhiteTexture();

            f32 projection[16];
            makeOrthographic(displaySize.x, displaySize.y, projection);

            if (projectionLocation >= 0) {
                glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, projection);
            }

            glDisable(GL_SCISSOR_TEST);

            for (const DrawCommand& command : drawList.span()) {
                handleCommand(command);
            }

            flushBatch();

            glDisable(GL_SCISSOR_TEST);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            glUseProgram(0);
        }

        void OpenGLBackend::handleCommand(const DrawCommand& command) {
            switch (command.type) {
                case DrawCommandType::Nop: {
                    break;
                }

                case DrawCommandType::PushClip: {
                    pushClip(command.rect);
                    break;
                }

                case DrawCommandType::PopClip: {
                    popClip();
                    break;
                }

                case DrawCommandType::Rect: {
                    drawRect(command);
                    break;
                }

                case DrawCommandType::RoundedRect: {
                    drawRoundedRect(command);
                    break;
                }

                case DrawCommandType::GradientRect: {
                    drawGradientRect(command);
                    break;
                }

                case DrawCommandType::Shadow: {
                    drawShadow(command);
                    break;
                }

                case DrawCommandType::TexturedRect: {
                    drawTexturedRect(command);
                    break;
                }

                case DrawCommandType::Line: {
                    drawLine(command);
                    break;
                }
            }
        }

        void OpenGLBackend::drawRect(const DrawCommand& command) {
            if (command.rect.isEmpty() || command.color.isTransparent()) {
                return;
            }
            
            if (command.thickness <= 0.0f) {
                appendRectFilled(command.rect, command.color);
            } else {
                appendRectOutline(command.rect, command.color, command.thickness);
            }
        }

        void OpenGLBackend::drawRoundedRect(const DrawCommand& command) {
            if (command.rect.isEmpty() || command.color.isTransparent()) {return;}
            
            if (command.thickness <= 0.0f) {
                appendRoundedRectFilled(command.rect, command.color, command.radius);
            } else {
                appendRoundedRectOutline(command.rect, command.color, command.radius, command.thickness);
            }
        }

        void OpenGLBackend::drawGradientRect(const DrawCommand& command) {
            if (command.radius > 0.0f) {
                appendGradientRoundedRectFilled(command.rect, command.gradient, command.radius);
                return;
            }

            appendGradientRectFilled(command.rect, command.gradient);
        }

        void OpenGLBackend::drawShadow(const DrawCommand& command) {
            appendSoftShadow(command.rect, command.color, command.radius, 
                command.shadowOffset, command.shadowBlur, command.shadowSpread);
        }

        void OpenGLBackend::drawTexturedRect(const DrawCommand& command) {
            if (command.rect.isEmpty() || command.uv.isEmpty()|| command.color.isTransparent()) {
                return;
            }

            ensureBatchTexture(command.texture);

            const Vec2 min = command.rect.min();
            const Vec2 max = command.rect.max();

            const f32 u0 = command.uv.x;
            const f32 v0 = command.uv.y;
            const f32 u1 = command.uv.x + command.uv.w;
            const f32 v1 = command.uv.y + command.uv.h;

            const Vec2 topLeft{min.x, min.y};
            const Vec2 topRight{max.x, min.y};
            const Vec2 bottomRight{max.x, max.y};
            const Vec2 bottomLeft{min.x, max.y};

            const Vec2 uvTopLeft{u0, v0};
            const Vec2 uvTopRight{u1, v0};
            const Vec2 uvBottomRight{u1, v1};
            const Vec2 uvBottomLeft{u0, v1};

            appendTexturedQuad(topLeft, topRight, bottomRight, bottomLeft, command.color, uvTopLeft, uvTopRight, uvBottomRight, uvBottomLeft);
        }

        void OpenGLBackend::drawLine(const DrawCommand& command) {
            if (command.color.isTransparent() || command.thickness <= 0.0f) {
                return;
            }

            appendLine(command.start, command.end, command.color, command.thickness);
        }

        void OpenGLBackend::appendSolidTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
            batchVertices.push_back(Vertex{a.x, a.y, color.r, color.g, color.b, color.a, 0.0f, 0.0f});
            batchVertices.push_back(Vertex{b.x, b.y, color.r, color.g, color.b, color.a, 0.0f, 0.0f});
            batchVertices.push_back(Vertex{c.x, c.y, color.r, color.g, color.b, color.a, 0.0f, 0.0f});
        }

        void OpenGLBackend::appendSolidQuad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) {
            appendSolidTriangle(a, b, c, color);
            appendSolidTriangle(a, c, d, color);
        }

        void OpenGLBackend::appendTexturedTriangle(Vec2 a, Vec2 b, Vec2 c, Color color, Vec2 uvA, Vec2 uvB, Vec2 uvC) {
            batchVertices.push_back(Vertex{a.x, a.y, color.r, color.g, color.b, color.a, uvA.x, uvA.y});
            batchVertices.push_back(Vertex{b.x, b.y, color.r, color.g, color.b, color.a, uvB.x, uvB.y});
            batchVertices.push_back(Vertex{c.x, c.y, color.r, color.g, color.b, color.a, uvC.x, uvC.y});
        }

        void OpenGLBackend::appendTexturedQuad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color, Vec2 uvA, Vec2 uvB, Vec2 uvC, Vec2 uvD) {
            appendTexturedTriangle(a, b, c, color, uvA, uvB, uvC);
            appendTexturedTriangle(a, c, d, color, uvA, uvC, uvD);
        }

        void OpenGLBackend::appendRectFilled(Rect rect, Color color) {
            if (rect.isEmpty() || color.isTransparent()) {return;}

            ensureBatchTexture(TextureHandle{});

            const Vec2 min = rect.min();
            const Vec2 max = rect.max();

            const Vec2 topLeft{min.x, min.y};
            const Vec2 topRight{max.x, min.y};
            const Vec2 bottomRight{max.x, max.y};
            const Vec2 bottomLeft{min.x, max.y};

            appendSolidQuad(topLeft, topRight, bottomRight, bottomLeft, color);
        }

        void OpenGLBackend::appendRectOutline(Rect rect, Color color, f32 thickness) {
            if (rect.isEmpty() || color.isTransparent() || thickness <= 0.0f) {return;}

            const Rect inner{rect.x + thickness, rect.y + thickness, rect.w - thickness * 2.0f, rect.h - thickness * 2.0f};

            if (inner.w <= 0.0f || inner.h <= 0.0f) {appendRectFilled(rect, color); return;}

            ensureBatchTexture(TextureHandle{});

            const Vec2 outerTopLeft{rect.x, rect.y};
            const Vec2 outerTopRight{rect.x + rect.w, rect.y};
            const Vec2 outerBottomRight{rect.x + rect.w, rect.y + rect.h};
            const Vec2 outerBottomLeft{rect.x, rect.y + rect.h};

            const Vec2 innerTopLeft{inner.x, inner.y};
            const Vec2 innerTopRight{inner.x + inner.w, inner.y};
            const Vec2 innerBottomRight{inner.x + inner.w, inner.y + inner.h};
            const Vec2 innerBottomLeft{inner.x, inner.y + inner.h};

            appendSolidQuad(outerTopLeft, outerTopRight, innerTopRight, innerTopLeft, color);
            appendSolidQuad(outerTopRight, outerBottomRight, innerBottomRight, innerTopRight, color);
            appendSolidQuad(outerBottomRight, outerBottomLeft, innerBottomLeft, innerBottomRight, color);
            appendSolidQuad(outerBottomLeft, outerTopLeft, innerTopLeft, innerBottomLeft, color);
        }

        void OpenGLBackend::appendRoundedRectFilled (Rect rect, Color color, f32 radius) {
            if (rect.isEmpty() || color.isTransparent()) {return;}

            radius = clampRadius(rect, radius);

            if (radius <= 0.0f) {
                appendRectFilled(rect, color);
                return;
            }

            Vec2 points[maxRoundedPoints];
            const usize count = buildRoundedRectPerimeter(rect, radius, points, static_cast<usize>(maxRoundedPoints));

            if (count < 3) { return;}

            ensureBatchTexture(TextureHandle{});

            const Vec2 center = rect.center();

            for (usize i = 0; i < count; ++i) {
                const usize next = (i + 1) % count;

                appendSolidTriangle(center, points[i], points[next], color);
            }
        }

        void OpenGLBackend::appendRoundedRectOutline(Rect rect, Color color, f32 radius, f32 thickness) {
            if (rect.isEmpty() || color.isTransparent() || thickness <= 0.0f) {return;}

            radius = clampRadius(rect, radius);

            if (radius <= 0.0f) {
                appendRectOutline(rect, color, thickness);
                return;
            }

            const Rect inner{rect.x + thickness, rect.y + thickness, rect.w - thickness * 2.0f, rect.h - thickness * 2.0f};

            if (inner.w <= 0.0f || inner.h <= 0.0f) {
                appendRoundedRectFilled(rect, color, radius);
                return;
            }

            const f32 innerRadius =  radius > thickness ? radius - thickness : 0.0f;

            Vec2 outerPoints[maxRoundedPoints];
            Vec2 innerPoints[maxRoundedPoints];

            const usize outerCount = buildRoundedRectPerimeter(rect, radius, outerPoints, static_cast<usize>(maxRoundedPoints));
            const usize innerCount = buildRoundedRectPerimeter(inner, innerRadius, innerPoints, static_cast<usize>(maxRoundedPoints));

            const usize count = outerCount < innerCount ? outerCount : innerCount;

            if (count < 3) { return;}
            
            ensureBatchTexture(TextureHandle{});

            for (usize i = 0; i < count; ++i) {
                const usize next = (i + 1) % count;

                appendSolidQuad(outerPoints[i], outerPoints[next], innerPoints[next], innerPoints[i], color);
            }
        }

        void OpenGLBackend::appendGradientRectFilled(Rect rect, const LinearGradient& gradient) {
            if (rect.isEmpty()) {return;}

            ensureBatchTexture(TextureHandle{});

            const Vec2 topLeft{rect.x, rect.y};
            const Vec2 topRight{rect.x + rect.w, rect.y};
            const Vec2 bottomRight{rect.x + rect.w, rect.y + rect.h};
            const Vec2 bottomLeft{rect.x, rect.y + rect.h};

            const Color c0 = gradientColorAt(topLeft, rect, gradient);
            const Color c1 = gradientColorAt(topRight, rect, gradient);
            const Color c2 = gradientColorAt(bottomRight, rect, gradient);
            const Color c3 = gradientColorAt(bottomLeft, rect, gradient);

            auto appendVertex = [&](Vec2 position, Color color) {
                batchVertices.push_back(Vertex{position.x, position.y, color.r, color.g, color.b, color.a, 0.0f, 0.0f});
            };

            appendVertex(topLeft, c0);
            appendVertex(topRight, c1);
            appendVertex(bottomRight, c2);

            appendVertex(topLeft, c0);
            appendVertex(bottomRight, c2);
            appendVertex(bottomLeft, c3);
        }

        void OpenGLBackend::appendGradientRoundedRectFilled(Rect rect, const LinearGradient& gradient, f32 radius) {
            if (rect.isEmpty()) {return;}

            radius = clampRadius(rect, radius);

            Vec2 points[maxRoundedPoints];
            const usize count = buildRoundedRectPerimeter(rect, radius, points, static_cast<usize>(maxRoundedPoints));

            if (count < 3) {return;}

            ensureBatchTexture(TextureHandle{});

            const Vec2 center = rect.center();
            const Color centerColor = gradientColorAt(center, rect, gradient);

            auto appendVertex = [&](Vec2 position, Color color) {
                batchVertices.push_back(Vertex{position.x, position.y, color.r, color.g, color.b, color.a, 0.0f, 0.0f});
            };

            for (usize i = 0; i < count; ++i) {
                const usize next = (i + 1) % count;

                appendVertex(center, centerColor);
                appendVertex(points[i], gradientColorAt(points[i], rect, gradient));
                appendVertex(points[next], gradientColorAt(points[next], rect, gradient));
            }
        }

        void OpenGLBackend::appendSoftShadow(Rect rect, Color color, f32 radius, Vec2 offset, f32 blur, f32 spread) {
            if (rect.isEmpty() || color.isTransparent()) {return;}

            if (blur < 0.0f) {blur = 0.0f;}

            const Rect inner{rect.x + offset.x - spread, rect.y + offset.y - spread, rect.w + spread * 2.0f, rect.h + spread * 2.0f};
            if (inner.isEmpty()) {return;}

            f32 innerRadius = radius + spread;
            if (innerRadius < 0.0f) {innerRadius = 0.0f;}
            innerRadius = clampRadius(inner, innerRadius);

            if (blur <= 0.0f) {
                appendRoundedRectFilled(inner, color, innerRadius);
                return;
            }

            const Rect outer{inner.x - blur, inner.y - blur, inner.w + blur * 2.0f, inner.h + blur * 2.0f};
            const f32 outerRadius = clampRadius(outer, innerRadius + blur);

            Vec2 innerPoints[maxShadowPoints];
            Vec2 outerPoints[maxShadowPoints];

            const usize innerCount = buildRoundedRectPerimeter(inner, innerRadius, innerPoints, static_cast<usize>(maxShadowPoints), shadowSegmentsPerCorner);
            const usize outerCount = buildRoundedRectPerimeter(outer, outerRadius, outerPoints, static_cast<usize>(maxShadowPoints), shadowSegmentsPerCorner);
            const usize count = innerCount < outerCount ? innerCount : outerCount;

            if (count < 3) {return;}

            ensureBatchTexture(TextureHandle{});

            const Vec2 center = inner.center();

            for (usize i = 0; i < count; ++i) {
                const usize next = (i + 1) % count;
                appendSolidTriangle(center, innerPoints[i], innerPoints[next], color);
            }

            const Color transparent{color.r, color.g, color.b, 0.0f};

            auto appendVertex = [&](Vec2 position, Color vertexColor) {
                batchVertices.push_back(Vertex{position.x, position.y, vertexColor.r, vertexColor.g, vertexColor.b, vertexColor.a, 0.0f, 0.0f});
            };

            for (usize i = 0; i < count; ++i) {
                const usize next = (i + 1) % count;

                appendVertex(innerPoints[i], color);
                appendVertex(innerPoints[next], color);
                appendVertex(outerPoints[next], transparent);

                appendVertex(innerPoints[i], color);
                appendVertex(outerPoints[next], transparent);
                appendVertex(outerPoints[i], transparent);
            }
        }

        void OpenGLBackend::appendLine(Vec2 start, Vec2 end, Color color, f32 thickness) {
            if (color.isTransparent() || thickness <= 0.0f) {return;}

            const f32 dx = end.x - start.x;
            const f32 dy = end.y - start.y;

            const f32 length = std::sqrt(dx * dx + dy * dy);

            if (length <= 0.0f) {return;}

            const f32 inverseLength = 1.0f / length;

            const f32 dirx = dx * inverseLength;
            const f32 diry = dy * inverseLength;

            const f32 normalx = -diry;
            const f32 normaly = dirx;

            const f32 offset = thickness * 0.5f;

            const f32 offsetx = offset * normalx;
            const f32 offsety = offset * normaly;

            const Vec2 a{start.x + offsetx, start.y + offsety};
            const Vec2 b{end.x + offsetx, end.y + offsety};
            const Vec2 c{end.x - offsetx, end.y - offsety};
            const Vec2 d{start.x - offsetx, start.y - offsety};

            ensureBatchTexture(TextureHandle{});
            appendSolidQuad(a, b, c, d, color);
        }

        void OpenGLBackend::ensureBatchTexture(TextureHandle texture) {
            if (!batchVertices.empty() &&  !sameTexture(currentBatchTexture, texture)) {flushBatch();}

            currentBatchTexture = texture;
        }

        void OpenGLBackend::flushBatch() {
            if (batchVertices.empty()) {return;}

            bindTexture(currentBatchTexture);

            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(batchVertices.size() * sizeof(Vertex)), batchVertices.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batchVertices.size()));

            batchVertices.clear();
        }

        void OpenGLBackend::pushClip(Rect rect) {
            flushBatch();

            Rect clip = rect;

            if (!clipStack.empty()) {
                clip = clipStack.back().intersection(rect);
            }

            clipStack.push_back(clip);
            applyScissor();
        }

        void OpenGLBackend::popClip() {
            flushBatch();

            if (!clipStack.empty()) {
                clipStack.pop_back();
            }

            applyScissor();
        }

        void OpenGLBackend::applyScissor() {
            if (clipStack.empty()) {
                glDisable(GL_SCISSOR_TEST);
                return;
            }

            const Rect rect = clipStack.back();

            f32 scaleX = 1.0f;
            f32 scaleY = 1.0f;

            if (displaySizeData.x > 0.0f) {
                scaleX = framebufferSizeData.x / displaySizeData.x;
            }

            if (displaySizeData.y > 0.0f) {
                scaleY = framebufferSizeData.y / displaySizeData.y;
            }

            f32 x = rect.x * scaleX;
            f32 y = rect.y * scaleY;
            f32 w = rect.w * scaleX;
            f32 h = rect.h * scaleY;

            const f32 framebufferWidth = framebufferSizeData.x;
            const f32 framebufferHeight = framebufferSizeData.y;

            if (w <= 0.0f || h <= 0.0f) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(0, 0, 0, 0);
                return;
            }

            f32 x2 = x + w;
            f32 y2 = y + h;

            if (x < 0.0f) {
                x = 0.0f;
            }

            if (y < 0.0f) {
                y = 0.0f;
            }

            if (x2 > framebufferWidth) {
                x2 = framebufferWidth;
            }

            if (y2 > framebufferHeight) {
                y2 = framebufferHeight;
            }

            const f32 clippedWidth = x2 - x;
            const f32 clippedHeight = y2 - y;

            if (clippedWidth <= 0.0f || clippedHeight <= 0.0f) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(0, 0, 0, 0);
                return;
            }

            const GLint scissorX = static_cast<GLint>(x);
            const GLint scissorY = static_cast<GLint>(framebufferHeight - y2);
            const GLsizei scissorWidth = static_cast<GLsizei>(clippedWidth);
            const GLsizei scissorHeight = static_cast<GLsizei>(clippedHeight);

            glEnable(GL_SCISSOR_TEST);
            glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
        }

        void OpenGLBackend::bindTexture(TextureHandle handle) {
            if (isValidTexture(handle)) {
                const TextureSlot& slot = textureSlots[handle.index];

                if (slot.id != 0) {glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(slot.id)); return;}
            }

            bindWhiteTexture();
        }

        void OpenGLBackend::bindWhiteTexture() {
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(whiteTexture));
        }

        bool OpenGLBackend::isValidTexture(TextureHandle handle) const {
            return handle.index < static_cast<u32>(textureSlots.size()) &&
                textureSlots[handle.index].alive &&
                textureSlots[handle.index].generation == handle.generation;
        }

        void OpenGLBackend::uploadTexture(TextureSlot& slot, u32 width, u32 height, const void* pixels, TextureFormat format) {
            if (slot.id == 0 || width == 0 || height == 0) {return;}
            
            GLuint id = static_cast<GLuint>(slot.id);
            glBindTexture(GL_TEXTURE_2D, id);

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            if (format == TextureFormat::RGBA8) {
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R, GL_RED);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

                glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA8, static_cast<GLsizei>(width), 
                    static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            } else {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_G, GL_ONE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B, GL_ONE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_A, GL_RED);

                glTexImage2D(GL_TEXTURE_2D,0,GL_R8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
            }

            slot.width = width;
            slot.height = height;
            slot.format = format;
        }
}
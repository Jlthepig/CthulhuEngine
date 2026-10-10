#pragma once

#include <vector>
#include "OctoGui/DrawList.h"
#include "OctoGui/TextureBackend.h"

namespace octogui
{
    class OpenGLBackend : public TextureBackend {
        
        public:
            struct Vertex {
                f32 x;
                f32 y;
                f32 r;
                f32 g;
                f32 b;
                f32 a;
                f32 u;
                f32 v;
            };

            bool init();

            void shutdown();

            void render(const DrawList& drawList, Vec2 displaySize, Vec2 framebufferSize);

            TextureHandle createTexture(u32 width, u32 height, const void* pixels, TextureFormat format) override;

            void updateTexture(TextureHandle handle, u32 width, u32 height, const void* pixels, TextureFormat format) override;
            void updateTextureRegion(TextureHandle, u32 x, u32 y, u32 width, u32 height, const void* pixels, u32 rowStrideBytes, TextureFormat format) override;

            void destroyTexture(TextureHandle handle) override;

        private:
            struct TextureSlot {
                u32 generation = 0;
                unsigned int id = 0;
                u32 width = 0;
                u32 height = 0;
                TextureFormat format = TextureFormat::RGBA8;
                bool alive = false;
            };
            
            void handleCommand(const DrawCommand& command);

            void drawRect(const DrawCommand& command);
            void drawRoundedRect(const DrawCommand& command);
            void drawGradientRect(const DrawCommand& command);
            void drawShadow(const DrawCommand& command);
            void drawTexturedRect(const DrawCommand& command);
            void drawLine(const DrawCommand& commmand);

            void appendSolidTriangle(Vec2 a, Vec2 b, Vec2 c, Color color);
            void appendSolidQuad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);

            void appendTexturedTriangle(Vec2 a, Vec2 b, Vec2 c, Color color, Vec2 uvA, Vec2 uvB, Vec2 uvC);
            void appendTexturedQuad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color, Vec2 uvA, Vec2 uvB, Vec2 uvC, Vec2 uvD);

            void appendRectFilled(Rect rect, Color color);
            void appendRectOutline(Rect rect, Color color, f32 thickness);

            void appendRoundedRectFilled(Rect rect, Color color, f32 radius);
            void appendRoundedRectOutline(Rect rect, Color color, f32 radius, f32 thickness);

            void appendGradientRectFilled(Rect rect, const LinearGradient& gradient);
            void appendGradientRoundedRectFilled(Rect rect, const LinearGradient& gradient, f32 radius);

            void appendSoftShadow(Rect rect, Color color, f32 radius, Vec2 offset, f32 blur, f32 spread);

            void appendLine(Vec2 start, Vec2 end, Color color, f32 thickness);

            void ensureBatchTexture(TextureHandle texture);
            void flushBatch();

            void pushClip(Rect rect);
            void popClip();
            void applyScissor();

            void bindTexture(TextureHandle handle);
            void bindWhiteTexture();

            [[nodiscard]]
            bool isValidTexture(TextureHandle handle) const;

            void uploadTexture(TextureSlot& slot, u32 width, u32 height, const void* pixels, TextureFormat format);

            unsigned int  program = 0;
            unsigned int  vao = 0;
            unsigned int vbo = 0;
            unsigned int whiteTexture = 0;

            int projectionLocation = -1;
            int textureLocation = -1;

            Vec2 displaySizeData{};
            Vec2 framebufferSizeData{};

            std::vector<Rect> clipStack;

            std::vector<TextureSlot> textureSlots;
            std::vector<u32> freeTextureSlots;

            std::vector<Vertex> batchVertices;
            TextureHandle currentBatchTexture{};
    };
}


#include "mesh.h"


namespace Cthulhu::Rendering
{
    void Mesh::setup(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
    {

        vertexData = vertices;
        indexData = indices;

        glGenVertexArrays(1,&VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1,&EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER,VBO);
        glBufferData(GL_ARRAY_BUFFER,vertices.size() * sizeof(float),vertices.data(),GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size() * sizeof(unsigned int),indices.data(),GL_STATIC_DRAW);

        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5 * sizeof(float), (void*) (3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0); 
    }
    
    void Mesh::draw()
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexData.size(), GL_UNSIGNED_INT,0);
        glBindVertexArray(0); 
    }
    
    void Mesh::destroy()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
}
#include <GLAD/glad/glad.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "LiteMath.h"
#include <iostream>
using namespace LiteMath;
#define STB_IMAGE_IMPLEMENTATION
#define GLFW_DLL
#define PI 3.14159265
#include "stb_image.h"
#include "common.h"


void OnKeyboardPressed(GLFWwindow* window, int key, int scancode, int action, int mode);
void OnMouseMove(GLFWwindow* window, double xpos, double ypos);
void OnMouseButtonClicked(GLFWwindow* window, int button, int action, int mods);
void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset);
void doCameraMovement(Camera& camera, GLfloat deltaTime);
unsigned int loadTexture(const char* path, bool gammaCorrection);

unsigned int SCR_WIDTH = 900;
unsigned int SCR_HEIGHT = 900;
static int filling = 0;
static bool keys[1024];
static bool g_captureMouse = true;
static bool g_capturedMouseJustNow = false;

bool bloom = true;
bool bloomKeyPressed = false;
float exposure = 1.0f;

Camera camera(glm::vec3(0.0f, 5.0f, 5.0f));//0.0 0.0 5.0
float lastX = 400;
float lastY = 300;
bool firstMouse = true;

float timer = 0.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

GLsizei CreateSphere(float radius, int numberSlices, GLuint& vao)
{
    int i, j;

    int numberParallels = numberSlices;
    int numberVertices = (numberParallels + 1) * (numberSlices + 1);
    int numberIndices = numberParallels * numberSlices * 3;

    float angleStep = (2.0f * PI) / ((float)numberSlices);

    std::vector<float> pos(numberVertices * 4, 0.0f);
    std::vector<float> norm(numberVertices * 4, 0.0f);
    std::vector<float> texcoords(numberVertices * 2, 0.0f);

    std::vector<int> indices(numberIndices, -1);

    for (i = 0; i < numberParallels + 1; i++)
    {
        for (j = 0; j < numberSlices + 1; j++)
        {
            int vertexIndex = (i * (numberSlices + 1) + j) * 4;
            int normalIndex = (i * (numberSlices + 1) + j) * 4;
            int texCoordsIndex = (i * (numberSlices + 1) + j) * 2;

            pos.at(vertexIndex + 0) = radius * sinf(angleStep * (float)i) * sinf(angleStep * (float)j);
            pos.at(vertexIndex + 1) = radius * cosf(angleStep * (float)i);
            pos.at(vertexIndex + 2) = radius * sinf(angleStep * (float)i) * cosf(angleStep * (float)j);
            pos.at(vertexIndex + 3) = 1.0f;

            norm.at(normalIndex + 0) = pos.at(vertexIndex + 0) / radius;
            norm.at(normalIndex + 1) = pos.at(vertexIndex + 1) / radius;
            norm.at(normalIndex + 2) = pos.at(vertexIndex + 2) / radius;
            norm.at(normalIndex + 3) = 1.0f;

            texcoords.at(texCoordsIndex + 0) = (float)j / (float)numberSlices;
            texcoords.at(texCoordsIndex + 1) = (1.0f - (float)i) / (float)(numberParallels - 1);
        }
    }

    int* indexBuf = &indices[0];

    for (i = 0; i < numberParallels; i++)
    {
        for (j = 0; j < numberSlices; j++)
        {
            *indexBuf++ = i * (numberSlices + 1) + j;
            *indexBuf++ = (i + 1) * (numberSlices + 1) + j;
            *indexBuf++ = (i + 1) * (numberSlices + 1) + (j + 1);

            *indexBuf++ = i * (numberSlices + 1) + j;
            *indexBuf++ = (i + 1) * (numberSlices + 1) + (j + 1);
            *indexBuf++ = i * (numberSlices + 1) + (j + 1);

            int diff = int(indexBuf - &indices[0]);
            if (diff >= numberIndices)
                break;
        }
        int     diff = int(indexBuf - &indices[0]);
        if (diff >= numberIndices)
            break;
    }

    GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(GLfloat), &pos[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, norm.size() * sizeof(GLfloat), &norm[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &vboTexCoords);
    glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords);
    glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(GLfloat), &texcoords[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}

GLsizei CreatePlane(GLuint& vao)
{
    std::vector<float> vertices = { 0.5f, 0.0f, 0.5f, 1.0f,
                                 0.5f, 0.0f,-0.5f, 1.0f,
                                -0.5f, 0.0f,-0.5f, 1.0f,
                                -0.5f, 0.0f, 0.5f, 1.0f };

    std::vector<float> normals = { 0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f };

    std::vector<uint32_t> indices = { 0u, 1u, 2u,
                                      0u, 3u, 2u };

    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}

GLsizei CreatePanel(GLuint& vao)
{
    std::vector<float> vertices = { 0.5f, 0.0f, 0.5f, 1.0f,
                                 0.5f, 0.0f, -0.5f, 1.0f,
                                -0.5f, 0.0f, 0.5f, 1.0f,
                                -0.5f, 0.0f, -0.5f, 1.0f };

    std::vector<float> normals = { 0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f };

    std::vector<uint32_t> indices = { 0u, 1u, 2u,
                                      1u, 2u, 3u };

    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}

GLsizei CreateCuboid(float lenght, float width, float height, GLuint& vao)
{
    std::vector <float> vertex = { 0.0f,0.0f,0.0f,1.0f,  0.0f,lenght,0.0f, 1.0f,  width,lenght,0.0f,1.0f,   width,0.0f,0.0f,1.0f,
                                   width,0.0f,height,1.0f,  width,lenght,height,1.0f,  0.0f,lenght,height,1.0f,  0.0f,0.0f,height,1.0f };
    std::vector <int> indices = { 0,1,2, 0,2,3, 2,3,4, 2,4,5, 4,5,6, 4,6,7, 7,0,4, 0,4,3, 6,7,1, 0,1,7, 1,2,6, 5,2,6 };

    std::vector <float> normal = { 0.0f,0.0f,-1.0f, 0.0f,0.0f,-1.0f, 1.0f,0.0f,0.0f, 1.0f,0.0f,0.0f,
                                  0.0f,0.0f,1.0f, 0.0f,0.0f,1.0f, 0.0f,-1.0f,0.0f, 0.0f,-1.0f,0.0f,
                                  -1.0f,0.0f,0.0f, -1.0f,0.0f,0.0f, 1.0f,0.0f,0.0f, 1.0f,0.0f,0.0f };



    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(GLfloat), vertex.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normal.size() * sizeof(GLfloat), normal.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}

GLsizei CreateConus(float radius, float height, int numberSlices, GLuint& vao)
{
    std::vector <float> vertex;//вершины
    std::vector <int> indices;//треугольники по индексам
    std::vector <float> normal;//нормали

    float xpos = 0.0f;
    float ypos = 0.0f;

    float angle = PI * 2.f / float(numberSlices); //угол смещения

    //центр дна
    vertex.push_back(xpos); vertex.push_back(0.0f);
    vertex.push_back(ypos);
    vertex.push_back(1.0f); //w

    //расчёт всех точек дна
    for (int i = 1; i <= numberSlices; i++)
    {
        float newX = radius * sinf(angle * i);
        float newY = -radius * cosf(angle * i);

        //для дна
        vertex.push_back(newX); vertex.push_back(0.0f);
        vertex.push_back(newY);

        vertex.push_back(1.0f); //w
    }

    //координата вершины конуса
    vertex.push_back(xpos); vertex.push_back(height);
    vertex.push_back(ypos);

    vertex.push_back(1.0f); //w

    //ИТОГО: вершины: центр основания, точка основания 1, точка основания 2,
    // и т.д., точка-вершина (четыре координаты)

    //расчёт поверхности дна + нормали
    for (int i = 1; i <= numberSlices; i++)
    {
        indices.push_back(0); //центр основания
        indices.push_back(i); //текущая точка

        if (i != numberSlices) //если не крайняя точка основания
        {
            indices.push_back(i + 1);//то соединяем со следующей по индексу
        }
        else
        {
            indices.push_back(1);//замыкаем с 1ой
        }

        //нормали у дна смотрят вниз
        normal.push_back(0.0f);
        normal.push_back(0.0f);
        normal.push_back(-1.0f);
    }
    //боковые поверхности + нормали
    for (int i = 1; i <= numberSlices; i++)
    {
        int k = 0;//нужно для нормалей

        indices.push_back(i);//текущая

        if (i != numberSlices) //если не крайняя точка основания
        {
            indices.push_back(i + 1);//то соединяем со следующей по индексу
            k = i + 1;
        }
        else
        {
            indices.push_back(1);//замыкаем с 1ой
            k = 1;
        }

        indices.push_back(numberSlices + 1);//вершина

        //расчет нормали к боковой
        float3 a, b, normalVec;
        //вектор а = координаты текущей - координаты вершины
        a.x = vertex[i * 4 - 3] - vertex[vertex.size() - 1 - 3];
        a.y = vertex[i * 4 - 2] - vertex[vertex.size() - 1 - 2];;
        a.z = vertex[i * 4 - 1] - vertex[vertex.size() - 1 - 1];;

        //вектор б = координаты седующей текущей (или 1 при последней итерации)
        // - координаты вершины)
        b.x = vertex[k * 4 - 3] - vertex[vertex.size() - 1 - 3];
        b.x = vertex[k * 4 - 2] - vertex[vertex.size() - 1 - 2];
        b.x = vertex[k * 4 - 1] - vertex[vertex.size() - 1 - 1];

        //нормаль как векторное произведение
        normalVec = cross(a, b);

        //запись нормаль в вектор
        normal.push_back(normalVec.x);
        normal.push_back(normalVec.y);
        normal.push_back(normalVec.z);
    }


    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(GLfloat), vertex.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normal.size() * sizeof(GLfloat), normal.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}

GLsizei CreateCylinder(float radius, float height, int numberSlices, GLuint& vao)
{

    float angle = 360 / numberSlices; //в градусах


    std::vector<float> position =
    {
       0.0f, 0.0f, 0.0f, 1.0f, //низ
        0.0f, 0.0f + height , 0.0f  , 1.0f, //верх
    };


    std::vector<float> normals =
    { 0.0f, 0.0f, 0.0f, 1.0f,
      1.0f, 1.0f, 0.0f, 1.0f };

    std::vector<uint32_t> indices;

    for (int i = 2; i < numberSlices + 2; i++) {
        // нижняя точка
        position.push_back(radius * cos(angle * i * PI / 180));  // x
        position.push_back(0.0f);                              // y
        position.push_back(radius * sin(angle * i * PI / 180)); // z
        position.push_back(1.0f);

        // верхняя точка
        position.push_back(radius * cos(angle * i * PI / 180));  // x
        position.push_back(height);                    // y
        position.push_back(radius * sin(angle * i * PI / 180)); // z
        position.push_back(1.0f);

        normals.push_back(position.at(i + 0) / numberSlices * 8.2);
        normals.push_back(position.at(i + 1) / numberSlices * 8.2);
        normals.push_back(position.at(i + 2) / numberSlices * 8.2);
        normals.push_back(1.0f);
        normals.push_back(position.at(i + 0) / numberSlices * 8.2);
        normals.push_back(position.at(i + 1) / numberSlices * 8.2);
        normals.push_back(position.at(i + 2) / numberSlices * 8.2);
        normals.push_back(1.0f);


    }

    for (int i = 2; i < numberSlices * 2 + 2; i = i + 2) {
        // нижнее основание
        indices.push_back(0);
        indices.push_back(i);
        if ((i) == numberSlices * 2)
        {
            indices.push_back(2);
        }
        else
        {
            indices.push_back(i + 2);
        }
        // верхнее основание
        indices.push_back(1);
        indices.push_back(i + 1);
        if ((i) == numberSlices * 2)
        {
            indices.push_back(3);
        }
        else
        {
            indices.push_back(i + 3);
        }

        indices.push_back(i);
        indices.push_back(i + 1);
        if ((i) == numberSlices * 2)
        {
            indices.push_back(2);
        }
        else
        {
            indices.push_back(i + 2);
        }

        indices.push_back(i + 1);
        if ((i) == numberSlices * 2)
        {
            indices.push_back(2);
            indices.push_back(3);
        }
        else
        {
            indices.push_back(i + 2);
            indices.push_back(i + 3);
        }

    }

    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, position.size() * sizeof(GLfloat), position.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}



unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}


int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 32);

    glfwWindowHint(GLFW_DECORATED, NULL);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWmonitor* MyMonitor = glfwGetPrimaryMonitor();

    const GLFWvidmode* mode = glfwGetVideoMode(MyMonitor);
    SCR_WIDTH = mode->width;
    SCR_HEIGHT = mode->height;

    GLFWwindow* window = glfwCreateWindow(glfwGetVideoMode(glfwGetPrimaryMonitor())->width, glfwGetVideoMode(glfwGetPrimaryMonitor())->height, "OpenGL basic sample", nullptr, nullptr);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);


    glfwSetKeyCallback(window, OnKeyboardPressed);
    glfwSetCursorPosCallback(window, OnMouseMove);
    glfwSetMouseButtonCallback(window, OnMouseButtonClicked);
    glfwSetScrollCallback(window, OnMouseScroll);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }


    glEnable(GL_DEPTH_TEST);

    Shader shader("../shader.vs", "../shader.fs");
    Shader skyboxShader("../skybox.vs", "../skybox.fs");
    Shader shaderLight("../shader.vs", "../light.fs");

    unsigned int sunTexture = loadTexture("../resources/textures/sun.jpg", true);
    unsigned int mercuryTexture = loadTexture("../resources/textures/mercury.jpg", true);
    unsigned int venusTexture = loadTexture("../resources/textures/venus.jpg", true);
    unsigned int earthTexture = loadTexture("../resources/textures/earth.jpg", true);
    unsigned int marsTexture = loadTexture("../resources/textures/mars.jpg", true);
    unsigned int jupiterTexture = loadTexture("../resources/textures/jupiter.jpg", true);
    unsigned int saturnTexture = loadTexture("../resources/textures/saturn.jpg", true);
    unsigned int uranusTexture = loadTexture("../resources/textures/uranus.jpg", true);
    unsigned int neptuneTexture = loadTexture("../resources/textures/neptune.jpg", true);
    unsigned int listTexture = loadTexture("../resources/textures/onyx.jpg", true);


    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    std::vector<std::string> faces
    {
       "..skybox/front.png", "..skybox/back.png", "..skybox/up.png", "..skybox/down.png", "..skybox/left.png", "..skybox/right.png"
    };
    unsigned int cubemapTexture = loadCubemap(faces);


    std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3(1.5f, 5.5f, 3.0f));
    lightPositions.push_back(glm::vec3(8.0f, 5.0f, 2.5f));
    lightPositions.push_back(glm::vec3(-4.5f, 6.5f, -1.5f));
    lightPositions.push_back(glm::vec3(5.0f, 10.0f, 0.0f));

    std::vector<glm::vec3> lightColors;
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));


    shader.use();
    shader.setInt("material.diffuse", 0);
    shader.setInt("material.specular", 1);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    ////////////////////////////
    GLuint vaoPlane;
    GLuint vaoSphere;
    GLuint vaoCuboid;
    GLuint vaoConus;
    GLuint vaoCylinder;
    GLuint vaoForest;
    GLuint vaoSolarSistem;
    GLuint vaoPanel;
    ////////////////////////////
    GLsizei sphereIndices = CreateSphere(1.0f, 50, vaoSphere);
    GLsizei Plane = CreatePlane(vaoPlane);
    GLsizei Panel = CreatePanel(vaoPanel);
    GLsizei Cuboid = CreateCuboid(0.5f, 0.4f, 0.6f, vaoCuboid);
    GLsizei Conus = CreateConus(1.0f, 3.0f, 12, vaoConus);
    GLsizei Cylinder = CreateCylinder(1.0f, 1.0f, 12, vaoCylinder);
    GLsizei Forest = CreateConus(1.0f, 3.0f, 72, vaoForest);
    GLsizei SolarSistem = CreateSphere(1.0f, 50, vaoSolarSistem);
    ////////////////////////////////////////////////////////////////////////////////


    GLuint vaoSphereLight1;
    GLsizei SphereLight1Indices = CreateSphere(0.03f, 30, vaoSphereLight1);
    GLuint vaoSphereLight2;
    GLsizei SphereLight2Indices = CreateSphere(0.06f, 30, vaoSphereLight2);
    GLuint vaoSphereLight3;
    GLsizei SphereLight3Indices = CreateSphere(0.05f, 30, vaoSphereLight3);
    GLuint vaoSphereLight4;
    GLsizei SphereLight4Indices = CreateSphere(1.0f, 30, vaoSphereLight4);
    GLuint vaoSun;
    GLsizei sunIndices = CreateSphere(7.0f, 50, vaoSun);
    GLuint vaoMerkur;
    GLsizei merkurIndices = CreateSphere(0.024f, 50, vaoMerkur);
    GLuint vaoVenera;
    GLsizei veneraIndices = CreateSphere(0.06f, 50, vaoVenera);
    GLuint vaoEarth;
    GLsizei earthIndices = CreateSphere(0.063f, 50, vaoEarth);
    GLuint vaoMars;
    GLsizei marsIndices = CreateSphere(0.034f, 50, vaoMars);
    GLuint vaoJupyter;
    GLsizei jupyterIndices = CreateSphere(0.7f, 50, vaoJupyter);
    GLuint vaoSaturn;
    GLsizei saturnIndices = CreateSphere(0.6f, 50, vaoSaturn);
    GLuint vaoUran;
    GLsizei uranIndices = CreateSphere(0.253f, 50, vaoUran);
    GLuint vaoNeptun;
    GLsizei neptunIndices = CreateSphere(0.246f, 50, vaoNeptun);
    GLuint vaoConus;
    GLsizei coneIndices = CreateConus(vaoConus, float4(0.0f, 0.0f, 0.0f, 1.0f), 3.0f, 8);
    GLuint vaoCylinder;
    GLsizei cylinderIndices = CreateCylinder(vaoCylinder, float4(0.0f, 0.0f, 0.0f, 1.0f), 20, 1.0f);


    while (!glfwWindowShouldClose(window))
    {
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        doCameraMovement(camera, deltaTime);

        glClearColor(0.4f, 0.25f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        shader.use();
        shader.setVec3("viewPos", camera.Position);
        shader.setFloat("material.shininess", 64.0f);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);

        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setMat4("model", model);


        shader.setVec3("dirLight.direction", 0.0f, -5.0f, 0.0f);
        shader.setVec3("dirLight.ambient", 0.6f, 0.6f, 0.6f);
        shader.setVec3("dirLight.color", 1.0f, 1.0f, 1.0f);
        shader.setVec3("dirLight.specular", 1.0f, 1.0f, 1.0f);

        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            shader.setVec3("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
            shader.setVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
            shader.setVec3("lights[" + std::to_string(i) + "].ambient", 0.5f, 0.5f, 0.5f);
            shader.setFloat("lights[" + std::to_string(i) + "].constant", 1.0f);
            shader.setFloat("lights[" + std::to_string(i) + "].linear", 0.09);
            shader.setFloat("lights[" + std::to_string(i) + "].quadratic", 0.032);
        }
        shader.setVec3("viewPos", camera.Position);
        glBindTexture(GL_TEXTURE_2D, listTexture);
        glBindVertexArray(vaoConus);
        {
            for (int i = 1; i < 50; i++) {

                model = glm::rotate(glm::mat4(1.0f), (i * 6 * 2.5f) * LiteMath::DEG_TO_RAD, glm::vec3(0.0, 1.0, 0.0));
                model = glm::translate(model, glm::vec3((i * 1.0f), -5.0f, +10.0f)); // пропорции
                model = glm::scale(model, glm::vec3(1.0f, 0.2f, 0.5f));
                shader.setMat4("model", model);

                glDrawElements(GL_TRIANGLE_STRIP, coneIndices, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
            }
        }

        glBindTexture(GL_TEXTURE_2D, sunTexture);
        glBindVertexArray(vaoSun);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, sunIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, mercuryTexture);
        glBindVertexArray(vaoMerkur);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 4.79f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(9.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, merkurIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, venusTexture);
        glBindVertexArray(vaoVenera);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 3.5f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(9.5f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, veneraIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, earthTexture);
        glBindVertexArray(vaoEarth);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 3.0f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(10.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, earthIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, marsTexture);
        glBindVertexArray(vaoMars);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 2.4f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(11.9f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, marsIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, jupiterTexture);
        glBindVertexArray(vaoJupyter);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 1.3f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(11.9f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, jupyterIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, saturnTexture);
        glBindVertexArray(vaoSaturn);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.96f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(13.5f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, saturnIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, uranusTexture);
        glBindVertexArray(vaoUran);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.68f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(14.8f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, uranIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, neptuneTexture);
        glBindVertexArray(vaoNeptun);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.54f * (float(PI) / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(15.5f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, neptunIndices, GL_UNSIGNED_INT, nullptr);
        }
      
    
        shaderLight.use();
        shaderLight.setMat4("projection", projection);
        shaderLight.setMat4("view", view);
        glBindVertexArray(vaoSphereLight1);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[0]));
            shaderLight.setMat4("model", model);
            shaderLight.setVec3("lightColor", lightColors[0]);
            glDrawElements(GL_TRIANGLE_STRIP, SphereLight1Indices, GL_UNSIGNED_INT, nullptr);
        }

        glBindVertexArray(vaoSphereLight2);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[1]));
            shaderLight.setMat4("model", model);
            shaderLight.setVec3("lightColor", lightColors[0]);
            glDrawElements(GL_TRIANGLE_STRIP, SphereLight2Indices, GL_UNSIGNED_INT, nullptr);
        }

        glBindVertexArray(vaoSphereLight3);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[2]));
            shaderLight.setMat4("model", model);
            shaderLight.setVec3("lightColor", lightColors[2]);
            glDrawElements(GL_TRIANGLE_STRIP, SphereLight3Indices, GL_UNSIGNED_INT, nullptr);
        }
        glBindVertexArray(vaoSphereLight4);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[3]));
            shaderLight.setMat4("model", model);
            shaderLight.setVec3("lightColor", lightColors[3]);
            glDrawElements(GL_TRIANGLE_STRIP, SphereLight4Indices, GL_UNSIGNED_INT, nullptr);
        }


        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);


        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        timer += 0.1;
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

   
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteVertexArrays(1, &vaoSun);
    glDeleteVertexArrays(1, &vaoMerkur);
    glDeleteVertexArrays(1, &vaoVenera);
    glDeleteVertexArrays(1, &vaoEarth);
    glDeleteVertexArrays(1, &vaoMars);
    glDeleteVertexArrays(1, &vaoJupyter);
    glDeleteVertexArrays(1, &vaoSaturn);
    glDeleteVertexArrays(1, &vaoUran);
    glDeleteVertexArrays(1, &vaoNeptun);
    glDeleteVertexArrays(1, &vaoConus); 
    glfwTerminate();
    return 0;
}


void OnKeyboardPressed(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
        if (action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);
        break;
    case GLFW_KEY_SPACE:
        if (action == GLFW_PRESS)
        {
            bloom = !bloom;
            bloomKeyPressed = true;
        }
        break;
    case GLFW_KEY_1:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
    case GLFW_KEY_2:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
    default:
        if (action == GLFW_PRESS)
            keys[key] = true;
        else if (action == GLFW_RELEASE)
            keys[key] = false;
    }
}


void OnMouseButtonClicked(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        g_captureMouse = !g_captureMouse;


    if (g_captureMouse)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_capturedMouseJustNow = true;
    }
    else
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}


void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = float(xpos);
        lastY = float(ypos);
        firstMouse = false;
    }

    GLfloat xoffset = float(xpos) - lastX;
    GLfloat yoffset = lastY - float(ypos);

    lastX = float(xpos);
    lastY = float(ypos);

    if (g_captureMouse)
        camera.ProcessMouseMovement(xoffset, yoffset);
}


void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(GLfloat(yoffset));
}

void doCameraMovement(Camera& camera, GLfloat deltaTime)
{
    if (keys[GLFW_KEY_W])
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (keys[GLFW_KEY_A])
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (keys[GLFW_KEY_S])
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (keys[GLFW_KEY_D])
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

//функция для загрузки тектуры
unsigned int loadTexture(char const* path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);//glGenTextures() в качестве входных данных принимает количество текстур, которые мы хотим создать, и сохраняет их в массиве типа unsigned int

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        //генерация текстуры
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
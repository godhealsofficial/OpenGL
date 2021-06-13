#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

//internal includes
#include "common.h"
#include "ShaderProgram.h"
#include "Camera.h"

//External dependencies
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <random>
#include "Texture.h"
#include "LiteMath.h"
#include <math.h>

//#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
//#include "tinyobjloader/tiny_obj_loader.h"

static const GLsizei WIDTH = 1920, HEIGHT = 1080; //размеры окна
static int filling = 0;
static bool keys[1024]; //массив состояний кнопок - нажата/не нажата
static GLfloat lastX = 400, lastY = 300; //исходное положение мыши
static bool firstMouse = true;
static bool g_captureMouse = true;  // Мышка захвачена нашим приложением или нет?
static bool g_capturedMouseJustNow = false;

GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

Camera camera(float3(0.0f, 15.0f, 30.0f));

//функция для обработки нажатий на кнопки клавиатуры
void OnKeyboardPressed(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	//std::cout << key << std::endl;
	switch (key)
	{
	case GLFW_KEY_ESCAPE: //на Esc выходим из программы
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GL_TRUE);
		break;
	case GLFW_KEY_SPACE: //на пробел переключение в каркасный режим и обратно
		if (action == GLFW_PRESS)
		{
			if (filling == 0)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				filling = 1;
			}
			else
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				filling = 0;
			}
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

//функция для обработки клавиш мыши
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

//функция для обработки перемещения мыши
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
		camera.ProcessMouseMove(xoffset, yoffset);
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

float3 CreateNormal(std::vector<float> vertex, int i, int k, int s)
{
	//расчет нормали к боковой
	float3 a, b;
	//вектор а
	a.x = vertex[i * 4] - vertex[s * 4];
	a.y = vertex[i * 4 + 1] - vertex[s * 4 + 1];
	a.z = vertex[i * 4 + 2] - vertex[s * 4 + 2];

	//вектор б 
	b.x = vertex[k * 4] - vertex[s * 4];
	b.x = vertex[k * 4 + 1] - vertex[s * 4 + 1];
	b.x = vertex[k * 4 + 2] - vertex[s * 4 + 2];

	//нормаль как векторное произведение
	return cross(a, b);
}

/**
\brief создать triangle strip плоскость и загрузить её в шейдерную программу
\param rows - число строк
\param cols - число столбцов
\param size - размер плоскости
\param vao - vertex array object, связанный с созданной плоскостью
*/
static int createTriStrip(int rows, int cols, float size, GLuint& vao)
{

	int numIndices = 2 * cols * (rows - 1) + rows - 1;

	std::vector<GLfloat> vertices_vec; //вектор атрибута координат вершин
	vertices_vec.reserve(rows * cols * 3);

	std::vector<GLfloat> normals_vec; //вектор атрибута нормалей к вершинам
	normals_vec.reserve(rows * cols * 3);

	std::vector<GLfloat> texcoords_vec; //вектор атрибут текстурных координат вершин
	texcoords_vec.reserve(rows * cols * 2);

	std::vector<float3> normals_vec_tmp(rows * cols, float3(0.0f, 0.0f, 0.0f)); //временный вектор нормалей, используемый для расчетов

	std::vector<int3> faces;         //вектор граней (треугольников), каждая грань - три индекса вершин, её составляющих; используется для удобства расчета нормалей
	faces.reserve(numIndices / 3);

	std::vector<GLuint> indices_vec; //вектор индексов вершин для передачи шейдерной программе
	indices_vec.reserve(numIndices);

	for (int z = 0; z < rows; ++z)
	{
		for (int x = 0; x < cols; ++x)
		{
			//вычисляем координаты каждой из вершин 
			float xx = -size / 2 + x * size / cols;
			float zz = -size / 2 + z * size / rows;
			// float yy = -1.0f;
			float r = sqrt(xx * xx + zz * zz);
			float yy = 5.0f * (r != 0.0f ? sin(r) / r : 1.0f);

			vertices_vec.push_back(xx);
			vertices_vec.push_back(yy);
			vertices_vec.push_back(zz);

			texcoords_vec.push_back(x / float(cols - 1)); // вычисляем первую текстурную координату u, для плоскости это просто относительное положение вершины
			texcoords_vec.push_back(z / float(rows - 1)); // аналогично вычисляем вторую текстурную координату v
		}
	}

	//primitive restart - специальный индекс, который обозначает конец строки из треугольников в triangle_strip
	//после этого индекса формирование треугольников из массива индексов начнется заново - будут взяты следующие 3 индекса для первого треугольника
	//и далее каждый последующий индекс будет добавлять один новый треугольник пока снова не встретится primitive restart index

	int primRestart = cols * rows;

	for (int x = 0; x < cols - 1; ++x)
	{
		for (int z = 0; z < rows - 1; ++z)
		{
			int offset = x * rows + z;

			//каждую итерацию добавляем по два треугольника, которые вместе формируют четырехугольник
			if (z == 0) //если мы в начале строки треугольников, нам нужны первые четыре индекса
			{
				indices_vec.push_back(offset + 0);
				indices_vec.push_back(offset + rows);
				indices_vec.push_back(offset + 1);
				indices_vec.push_back(offset + rows + 1);
			}
			else // иначе нам достаточно двух индексов, чтобы добавить два треугольника
			{
				indices_vec.push_back(offset + 1);
				indices_vec.push_back(offset + rows + 1);

				if (z == rows - 2) indices_vec.push_back(primRestart); // если мы дошли до конца строки, вставляем primRestart, чтобы обозначить переход на следующую строку
			}
		}
	}

	///////////////////////
	//формируем вектор граней(треугольников) по 3 индекса на каждый
	int currFace = 1;
	for (int i = 0; i < indices_vec.size() - 2; ++i)
	{
		int3 face;

		int index0 = indices_vec.at(i);
		int index1 = indices_vec.at(i + 1);
		int index2 = indices_vec.at(i + 2);

		if (index0 != primRestart && index1 != primRestart && index2 != primRestart)
		{
			if (currFace % 2 != 0) //если это нечетный треугольник, то индексы и так в правильном порядке обхода - против часовой стрелки
			{
				face.x = indices_vec.at(i);
				face.y = indices_vec.at(i + 1);
				face.z = indices_vec.at(i + 2);

				currFace++;
			}
			else //если треугольник четный, то нужно поменять местами 2-й и 3-й индекс;
			{    //при отрисовке opengl делает это за нас, но при расчете нормалей нам нужно это сделать самостоятельно
				face.x = indices_vec.at(i);
				face.y = indices_vec.at(i + 2);
				face.z = indices_vec.at(i + 1);

				currFace++;
			}
			faces.push_back(face);
		}
	}


	///////////////////////
	//расчет нормалей
	for (int i = 0; i < faces.size(); ++i)
	{
		//получаем из вектора вершин координаты каждой из вершин одного треугольника
		float3 A(vertices_vec.at(3 * faces.at(i).x + 0), vertices_vec.at(3 * faces.at(i).x + 1), vertices_vec.at(3 * faces.at(i).x + 2));
		float3 B(vertices_vec.at(3 * faces.at(i).y + 0), vertices_vec.at(3 * faces.at(i).y + 1), vertices_vec.at(3 * faces.at(i).y + 2));
		float3 C(vertices_vec.at(3 * faces.at(i).z + 0), vertices_vec.at(3 * faces.at(i).z + 1), vertices_vec.at(3 * faces.at(i).z + 2));

		//получаем векторы для ребер треугольника из каждой из 3-х вершин
		float3 edge1A(normalize(B - A));
		float3 edge2A(normalize(C - A));

		float3 edge1B(normalize(A - B));
		float3 edge2B(normalize(C - B));

		float3 edge1C(normalize(A - C));
		float3 edge2C(normalize(B - C));

		//нормаль к треугольнику - векторное произведение любой пары векторов из одной вершины
		float3 face_normal = cross(edge1A, edge2A);

		//простой подход: нормаль к вершине = средняя по треугольникам, к которым принадлежит вершина
		normals_vec_tmp.at(faces.at(i).x) += face_normal;
		normals_vec_tmp.at(faces.at(i).y) += face_normal;
		normals_vec_tmp.at(faces.at(i).z) += face_normal;
	}

	//нормализуем векторы нормалей и записываем их в вектор из GLFloat, который будет передан в шейдерную программу
	for (int i = 0; i < normals_vec_tmp.size(); ++i)
	{
		float3 N = normalize(normals_vec_tmp.at(i));

		normals_vec.push_back(N.x);
		normals_vec.push_back(N.y);
		normals_vec.push_back(N.z);
	}


	GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vboVertices);
	glGenBuffers(1, &vboIndices);
	glGenBuffers(1, &vboNormals);
	glGenBuffers(1, &vboTexCoords);


	glBindVertexArray(vao); GL_CHECK_ERRORS;
	{

		//передаем в шейдерную программу атрибут координат вершин
		glBindBuffer(GL_ARRAY_BUFFER, vboVertices); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, vertices_vec.size() * sizeof(GLfloat), &vertices_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(0); GL_CHECK_ERRORS;

		//передаем в шейдерную программу атрибут нормалей
		glBindBuffer(GL_ARRAY_BUFFER, vboNormals); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, normals_vec.size() * sizeof(GLfloat), &normals_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(1); GL_CHECK_ERRORS;

		//передаем в шейдерную программу атрибут текстурных координат
		glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, texcoords_vec.size() * sizeof(GLfloat), &texcoords_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(2); GL_CHECK_ERRORS;

		//передаем в шейдерную программу индексы
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices); GL_CHECK_ERRORS;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_vec.size() * sizeof(GLuint), &indices_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;

		glEnable(GL_PRIMITIVE_RESTART); GL_CHECK_ERRORS;
		glPrimitiveRestartIndex(primRestart); GL_CHECK_ERRORS;
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);


	return numIndices;
}

GLsizei CreateCone(float radius, float height, int numberSlices, GLuint& vao)
{
	std::vector <float> vertex;//вершины
	std::vector <int> faces;//треугольники по индексам
	std::vector <float> normal;//нормали

	float xpos = 0.0f;
	float ypos = 0.0f;

	float angle = M_PI * 2.f / float(numberSlices); //угол смещения

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
		faces.push_back(0); //центр основания
		faces.push_back(i); //текущая точка

		if (i != numberSlices) //если не крайняя точка основания
		{
			faces.push_back(i + 1);//то соединяем со следующей по индексу
		}
		else
		{
			faces.push_back(1);//замыкаем с 1ой
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

		faces.push_back(i);//текущая

		if (i != numberSlices) //если не крайняя точка основания
		{
			faces.push_back(i + 1);//то соединяем со следующей по индексу
			k = i + 1;
		}
		else
		{
			faces.push_back(1);//замыкаем с 1ой
			k = 1;
		}

		faces.push_back(numberSlices + 1);//вершина

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
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(int), faces.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);

	return faces.size();
}

GLsizei CreateCub(float lenght, float width, float height, GLuint& vao)
{
	std::vector <float> vertex = { 0.0f,0.0f,0.0f,1.0f,  0.0f,lenght,0.0f, 1.0f,  width,lenght,0.0f,1.0f,   width,0.0f,0.0f,1.0f,
								   width,0.0f,height,1.0f,  width,lenght,height,1.0f,  0.0f,lenght,height,1.0f,  0.0f,0.0f,height,1.0f };
	std::vector <int> faces = { 0,1,2, 0,2,3, 2,3,4, 2,4,5, 4,5,6, 4,6,7, 7,0,4, 0,4,3, 6,7,1, 0,1,7, 1,2,6, 5,2,6 };
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
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(int), faces.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);

	return faces.size();
}


GLsizei CreateSphere(float radius, int numberSlices, GLuint& vao)
{
	int i, j;

	int numberParallels = numberSlices;
	int numberVertices = (numberParallels + 1) * (numberSlices + 1);
	int numberIndices = numberParallels * numberSlices * 3;

	float angleStep = (2.0f * M_PI) / ((float)numberSlices);

	std::vector<float> pos(numberVertices * 4, 0.0f);//координаты
	std::vector<float> norm(numberVertices * 4, 0.0f);
	std::vector<float> texcoords(numberVertices * 2, 0.0f);//для текстур

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
		int diff = int(indexBuf - &indices[0]);
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

GLsizei createTriangle(GLuint& vao)
{

	std::vector<GLfloat> vertices_vec = { -0.5f, -0.5f, +0.0f,
										 +0.5f, -0.5f, +0.0f,
										 +0.0f, +0.5f, +0.0f };

	std::vector<GLfloat> normals_vec = { 0.0f, 0.0f, 1.0f,
										0.0f, 0.0f, 1.0f,
										0.0f, 0.0f, 1.0f };

	std::vector<GLfloat> texcoords_vec = { 0.0f, 0.0f,
										  1.0f, 0.0f,
										  0.5f, 0.5f };

	std::vector<GLuint> indices_vec = { 0, 1, 2 };


	GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vboVertices);
	glGenBuffers(1, &vboIndices);
	glGenBuffers(1, &vboNormals);
	glGenBuffers(1, &vboTexCoords);

	glBindVertexArray(vao); GL_CHECK_ERRORS;
	{
		//передаем в шейдерную программу атрибут координат вершин
		glBindBuffer(GL_ARRAY_BUFFER, vboVertices); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, vertices_vec.size() * sizeof(GLfloat), &vertices_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(0); GL_CHECK_ERRORS;

		//передаем в шейдерную программу атрибут нормалей
		glBindBuffer(GL_ARRAY_BUFFER, vboNormals); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, normals_vec.size() * sizeof(GLfloat), &normals_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(1); GL_CHECK_ERRORS;

		//передаем в шейдерную программу атрибут текстурных координат
		glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, texcoords_vec.size() * sizeof(GLfloat), &texcoords_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(2); GL_CHECK_ERRORS;

		//передаем в шейдерную программу индексы
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices); GL_CHECK_ERRORS;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_vec.size() * sizeof(GLuint), &indices_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;

	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	return indices_vec.size();
}

GLsizei createCube(GLuint& vao)
{

	std::vector<GLfloat> vertices_vec = { -1.0f, -1.0f, -1.0f,
										 -1.0f, -1.0f, +1.0f,
										 +1.0f, -1.0f, +1.0f,
										 +1.0f, -1.0f, -1.0f,

										 -1.0f, +1.0f, -1.0f,
										 -1.0f, +1.0f, +1.0f,
										 +1.0f, +1.0f, +1.0f,
										 +1.0f, +1.0f, -1.0f,

										 -1.0f, -1.0f, -1.0f,
										 -1.0f, +1.0f, -1.0f,
										 +1.0f, +1.0f, -1.0f,
										 +1.0f, -1.0f, -1.0f,

										 -1.0f, -1.0f, +1.0f,
										 -1.0f, +1.0f, +1.0f,
										 +1.0f, +1.0f, +1.0f,
										 +1.0f, -1.0f, +1.0f,

										 -1.0f, -1.0f, -1.0f,
										 -1.0f, -1.0f, +1.0f,
										 -1.0f, +1.0f, +1.0f,
										 -1.0f, +1.0f, -1.0f,

										 +1.0f, -1.0f, -1.0f,
										 +1.0f, -1.0f, +1.0f,
										 +1.0f, +1.0f, +1.0f,
										 +1.0f, +1.0f, -1.0f };

	std::vector<GLfloat> normals_vec = { 0.0f, -1.0f, 0.0f,
											0.0f, -1.0f, 0.0f,
											0.0f, -1.0f, 0.0f,
											0.0f, -1.0f, 0.0f,

											0.0f, +1.0f, 0.0f,
											0.0f, +1.0f, 0.0f,
											0.0f, +1.0f, 0.0f,
											0.0f, +1.0f, 0.0f,

											0.0f, 0.0f, -1.0f,
											0.0f, 0.0f, -1.0f,
											0.0f, 0.0f, -1.0f,
											0.0f, 0.0f, -1.0f,

											0.0f, 0.0f, +1.0f,
											0.0f, 0.0f, +1.0f,
											0.0f, 0.0f, +1.0f,
											0.0f, 0.0f, +1.0f,

											-1.0f, 0.0f, 0.0f,
											-1.0f, 0.0f, 0.0f,
											-1.0f, 0.0f, 0.0f,
											-1.0f, 0.0f, 0.0f,

											+1.0f, 0.0f, 0.0f,
											+1.0f, 0.0f, 0.0f,
											+1.0f, 0.0f, 0.0f,
											+1.0f, 0.0f, 0.0f };

	std::vector<GLfloat> texcoords_vec = { 0.0f, 0.0f,
											 0.0f, 1.0f,
											 1.0f, 1.0f,
											 1.0f, 0.0f,

											 1.0f, 0.0f,
											 1.0f, 1.0f,
											 0.0f, 1.0f,
											 0.0f, 0.0f,

											 0.0f, 0.0f,
											 0.0f, 1.0f,
											 1.0f, 1.0f,
											 1.0f, 0.0f,

											 0.0f, 0.0f,
											 0.0f, 1.0f,
											 1.0f, 1.0f,
											 1.0f, 0.0f,

											 0.0f, 0.0f,
											 0.0f, 1.0f,
											 1.0f, 1.0f,
											 1.0f, 0.0f,

											 0.0f, 0.0f,
											 0.0f, 1.0f,
											 1.0f, 1.0f,
											 1.0f, 0.0f, };

	std::vector<GLuint> indices_vec = { 0, 2, 1,
										   0, 3, 2,

										   4, 5, 6,
										   4, 6, 7,

											8, 9, 10,
											8, 10, 11,

										   12, 15, 14,
										   12, 14, 13,

										   16, 17, 18,
										   16, 18, 19,

										   20, 23, 22,
										   20, 22, 21 };


	GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vboVertices);
	glGenBuffers(1, &vboIndices);
	glGenBuffers(1, &vboNormals);
	glGenBuffers(1, &vboTexCoords);

	glBindVertexArray(vao); GL_CHECK_ERRORS;
	{
		//передаем в шейдерную программу атрибут координат вершин
		glBindBuffer(GL_ARRAY_BUFFER, vboVertices); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, vertices_vec.size() * sizeof(GLfloat), &vertices_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(0); GL_CHECK_ERRORS;

		//передаем в шейдерную программу атрибут нормалей
		glBindBuffer(GL_ARRAY_BUFFER, vboNormals); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, normals_vec.size() * sizeof(GLfloat), &normals_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(1); GL_CHECK_ERRORS;

		//передаем в шейдерную программу атрибут текстурных координат
		glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords); GL_CHECK_ERRORS;
		glBufferData(GL_ARRAY_BUFFER, texcoords_vec.size() * sizeof(GLfloat), &texcoords_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (nullptr)); GL_CHECK_ERRORS;
		glEnableVertexAttribArray(2); GL_CHECK_ERRORS;

		//передаем в шейдерную программу индексы
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices); GL_CHECK_ERRORS;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_vec.size() * sizeof(GLuint), &indices_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;

	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	return indices_vec.size();
}


GLsizei CreateCylinder(float radius, float height, int numberSlices, GLuint& vao)
{
	std::vector <float> vertex;
	std::vector <int> faces;
	std::vector <float> normal;

	float xpos = 0.0f;
	float ypos = 0.0f;

	float angle = M_PI * 2.f / float(numberSlices);

	//центр дна
	vertex.push_back(xpos);
	vertex.push_back(ypos);
	vertex.push_back(0.0f);
	vertex.push_back(1.0f); //w

	//центр крышки
	vertex.push_back(xpos);
	vertex.push_back(ypos);
	vertex.push_back(height);
	vertex.push_back(1.0f); //w

	//расчёт всех точек
	for (int i = 1; i <= numberSlices; i++)
	{
		float newX = radius * sinf(angle * i);
		float newY = -radius * cosf(angle * i);

		//для дна
		vertex.push_back(newX);
		vertex.push_back(newY);
		vertex.push_back(0.0f);
		vertex.push_back(1.0f); //w

		//для крышки
		vertex.push_back(newX);
		vertex.push_back(newY);
		vertex.push_back(height);
		vertex.push_back(1.0f); //w
	}
	//расчёт поверхностей крышки и дна
	for (int i = 0; i <= 2 * numberSlices - 1; i++)
	{
		//индексы с четными номерами принадлежат дну
		//с нечетными - крышке
		if (i % 2 == 0)//дно
		{
			faces.push_back(0);//центр дна

			//нормали смотрят вниз
			normal.push_back(0.0f);
			normal.push_back(0.0f);
			normal.push_back(-1.0f);
		}
		else//крышка
		{
			faces.push_back(1);//центр крышки

			//нормали смотрят вверх
			normal.push_back(0.0f);
			normal.push_back(0.0f);
			normal.push_back(1.0f);
		}

		faces.push_back(i + 2);//следующая вершина после центра

		//завершающая вершина
		if (((i + 4) % (2 * numberSlices) == 0.0f) || ((i + 4) % (2 * numberSlices) == 1.0f))
		{
			faces.push_back(i + 4);
		}
		else
		{
			faces.push_back((i + 4) % (2 * numberSlices));
		}
	}
	//боковые поверхности
	for (int i = 2; i <= 2 * numberSlices; i = i + 2)
	{
		if ((i != 2) && (i != 2 * numberSlices))
		{
			//первый треугольник
			faces.push_back(i);
			faces.push_back(i + 1);
			faces.push_back(i + 2);

			normal.push_back(CreateNormal(vertex, (i + 1), (i + 2), i).x);
			normal.push_back(CreateNormal(vertex, (i + 1), (i + 2), i).y);
			normal.push_back(CreateNormal(vertex, (i + 1), (i + 2), i).z);

			//каждая четная вершина учавствует в двух треугольниках
			//(кроме второй)
			//второй треугольник
			faces.push_back(i);
			faces.push_back(i - 1);
			faces.push_back(i + 1);

			normal.push_back(CreateNormal(vertex, (i + 1), (i - 1), i).x);
			normal.push_back(CreateNormal(vertex, (i + 1), (i - 1), i).y);
			normal.push_back(CreateNormal(vertex, (i + 1), (i - 1), i).z);
		}
		else if (i == 2)
		{
			//первый треугольник
			faces.push_back(i);
			faces.push_back(i + 1);
			faces.push_back(i + 2);

			normal.push_back(CreateNormal(vertex, (i + 1), (i + 2), i).x);
			normal.push_back(CreateNormal(vertex, (i + 1), (i + 2), i).y);
			normal.push_back(CreateNormal(vertex, (i + 1), (i + 2), i).z);
		}
		else if (i == 2 * numberSlices)
		{
			//первый треугольник
			faces.push_back(i);
			faces.push_back(i + 1);
			faces.push_back(3);

			normal.push_back(CreateNormal(vertex, (i + 1), 3, i).x);
			normal.push_back(CreateNormal(vertex, (i + 1), 3, i).y);
			normal.push_back(CreateNormal(vertex, (i + 1), 3, i).z);

			//второй треугольник
			faces.push_back(i);
			faces.push_back(2);
			faces.push_back(3);

			normal.push_back(CreateNormal(vertex, 2, 3, i).x);
			normal.push_back(CreateNormal(vertex, 2, 3, i).y);
			normal.push_back(CreateNormal(vertex, 2, 3, i).z);
		}

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
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(int), faces.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);

	return faces.size();
}

GLsizei CreateSquarePlane(GLuint& vao)
{
	std::vector<float> vertices = { 0.5f, 0.0f, 0.5f, 1.0f,
								 0.5f, 0.0f,-0.5f, 1.0f,
								-0.5f, 0.0f,-0.5f, 1.0f,
								-0.5f, 0.0f, 0.5f, 1.0f };

	//std::vector<float> normals = findNormal(vertices);

	std::vector<float> normals = { 0.0f, 0.0f, -1.0f, 1.0f,
								   0.0f, 0.0f, -1.0f, 1.0f,
								   0.0f, 0.0f, -1.0f, 1.0f,
								   0.0f, 0.0f, -1.0f, 1.0f };

	std::vector<uint32_t> indices = { 0u, 1u, 2u,
									  0u, 3u, 2u };

	std::vector<float> texcoords_vec = { 0.0f, 0.0f,
										 0.0f, 1.0f,
										 1.0f, 1.0f,
										 1.0f, 0.0f, };



	GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

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

	glGenBuffers(1, &vboTexCoords);
	//передаем в шейдерную программу атрибут текстурных координат
	glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords); GL_CHECK_ERRORS;
	glBufferData(GL_ARRAY_BUFFER, texcoords_vec.size() * sizeof(GLfloat), &texcoords_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (nullptr)); GL_CHECK_ERRORS;
	glEnableVertexAttribArray(2); GL_CHECK_ERRORS;


	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);

	return indices.size();
}

GLsizei CreateCube2(GLuint& vao)
{
	std::vector<float> vertices = {       //низ
										 -1.0f, -1.0f, -1.0f, // 1
										 -1.0f, -1.0f, +1.0f, // 2
										 +1.0f, -1.0f, +1.0f, // 3
										 +1.0f, -1.0f, -1.0f, // 4

										 //верх
										 -1.0f, +1.0f, -1.0f, // 5
										 -1.0f, +1.0f, +1.0f, // 6
										 +1.0f, +1.0f, +1.0f, // 7
										 +1.0f, +1.0f, -1.0f, // 8

										 -1.0f, -1.0f, -1.0f, // 1
										 -1.0f, +1.0f, -1.0f, // 5
										 +1.0f, +1.0f, -1.0f, // 8
										 +1.0f, -1.0f, -1.0f, // 4

										 -1.0f, -1.0f, +1.0f, // 2
										 -1.0f, +1.0f, +1.0f, // 6
										 +1.0f, +1.0f, +1.0f, // 7
										 +1.0f, -1.0f, +1.0f, // 3

										 -1.0f, -1.0f, -1.0f, // 1
										 -1.0f, -1.0f, +1.0f, // 2
										 -1.0f, +1.0f, +1.0f, // 6
										 -1.0f, +1.0f, -1.0f, // 5

										 +1.0f, -1.0f, -1.0f, // 4
										 +1.0f, -1.0f, +1.0f, // 3
										 +1.0f, +1.0f, +1.0f, // 7
										 +1.0f, +1.0f, -1.0f  // 8
	};

	//std::vector<float> normals = findNormal(vertices); // ?

	std::vector<float> normals = { 0.0f, -1.0f, 0.0f,
											0.0f, -1.0f, 0.0f,
											0.0f, -1.0f, 0.0f,
											0.0f, -1.0f, 0.0f,

											0.0f, +1.0f, 0.0f,
											0.0f, +1.0f, 0.0f,
											0.0f, +1.0f, 0.0f,
											0.0f, +1.0f, 0.0f,

											0.0f, 0.0f, -1.0f,
											0.0f, 0.0f, -1.0f,
											0.0f, 0.0f, -1.0f,
											0.0f, 0.0f, -1.0f,

											0.0f, 0.0f, +1.0f,
											0.0f, 0.0f, +1.0f,
											0.0f, 0.0f, +1.0f,
											0.0f, 0.0f, +1.0f,

											-1.0f, 0.0f, 0.0f,
											-1.0f, 0.0f, 0.0f,
											-1.0f, 0.0f, 0.0f,
											-1.0f, 0.0f, 0.0f,

											+1.0f, 0.0f, 0.0f,
											+1.0f, 0.0f, 0.0f,
											+1.0f, 0.0f, 0.0f,
											+1.0f, 0.0f, 0.0f };

	std::vector<uint32_t> indices = { 0, 2, 1,
										   0, 3, 2,

										   4, 5, 6,
										   4, 6, 7,

											8, 9, 10,
											8, 10, 11,

										   12, 15, 14,
										   12, 14, 13,

										   16, 17, 18,
										   16, 18, 19,

										   20, 23, 22,
										   20, 22, 21 };


	std::vector<GLfloat> texcoords_vec = { 0.5f, 0.38f,
											 0.5f, 0.60f,
											 0.25f, 0.60f,
											 0.25f, 0.38f,

											 0.75f, 0.38f,
											 0.75f, 0.60f,
											 1.0f, 0.60f,
											 1.0f, 0.38f,

											 0.5f, 0.38f,
											 0.5f, 0.0f,
											 0.25f, 0.0f,
											 0.25f, 0.38f,//4

											 0.5f, 0.60f,
											 0.5f, 1.0f,
											 0.25f, 1.0f,
											 0.25f, 0.60f,//3

											 0.5f, 0.38f,
											 0.5f, 0.60f,
											 0.75f, 0.60f,
											 0.75f, 0.38f,

											 0.25f, 0.38f,//4
											 0.25f, 0.60f,//3
											 0.0f, 0.60f,//7
											 0.0f, 0.38f, };//8
	GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vboIndices);

	glBindVertexArray(vao);

	glGenBuffers(1, &vboVertices);
	glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &vboNormals);
	glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
	glEnableVertexAttribArray(1);

	//передаем в шейдерную программу атрибут текстурных координат
	glGenBuffers(1, &vboTexCoords);
	glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords); GL_CHECK_ERRORS;
	glBufferData(GL_ARRAY_BUFFER, texcoords_vec.size() * sizeof(GLfloat), &texcoords_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (nullptr)); GL_CHECK_ERRORS;
	glEnableVertexAttribArray(2); GL_CHECK_ERRORS;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);

	return indices.size();
}

GLsizei CreateTable(GLuint& vao)
{
	std::vector<float> vertices = { 0.5f, 1.1f, 0.5f, 1.0f,
									0.5f, 1.1f,-0.5f, 1.0f,
								   -0.5f, 1.1f,-0.5f, 1.0f,
								   -0.5f, 1.1f, 0.5f, 1.0f,

									0.5f, 1.0f, 0.5f, 1.0f,
									0.5f, 1.0f,-0.5f, 1.0f,
								   -0.5f, 1.0f,-0.5f, 1.0f,
								   -0.5f, 1.0f, 0.5f, 1.0f,

								   -0.45f, 1.1f,-0.45f, 1.0f,
								   -0.45f, 1.1f, 0.45f, 1.0f,
								   -0.45f, 0.0f, 0.45f, 1.0f,
								   -0.45f, 0.0f,-0.45f, 1.0f,

									0.45f, 1.1f,-0.45f, 1.0f,
									0.45f, 1.1f, 0.45f, 1.0f,
									0.45f, 0.0f, 0.45f, 1.0f,
									0.45f, 0.0f,-0.45f, 1.0f,
	};


	std::vector<float> normals = { 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 1.0f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f,
								 0.0f, 0.0f, 0.4f, 1.0f, };

	std::vector<uint32_t> indices = { 0u, 1u, 2u,
									  0u, 3u, 2u,
									  3u, 2u, 6u,
									  3u, 7u, 6u,
									  0u, 3u, 7u,
									  0u, 4u, 7u,
									  1u, 2u, 6u,
									  1u, 5u, 6u,
									  4u, 7u, 6u,
									  4u, 5u, 6u,
									  0u, 1u, 5u,
									  0u, 4u, 5u,
									  8u, 9u, 10u,
									  8u, 11u, 10u,
									  12u, 13u, 14u,
									  12u, 15u, 14u,
									  8u, 11u, 12u,
									  15u, 11u, 12u,
	};

	std::vector<GLfloat> texcoords_vec = { 0.0f, 0.0f,
										 0.0f, 1.0f,
										 1.0f, 1.0f,
										 1.0f, 0.0f,

										 0.0f, 0.0f,
										 0.0f, 1.0f,
										 1.0f, 1.0f,
										 1.0f, 0.0f,

										  1.0f, 0.0f,
										 1.0f, 1.0f,
										 0.0f, 1.0f,
										 0.0f, 0.0f,

										 1.0f, 0.0f,
										 1.0f, 1.0f,
										 0.0f, 1.0f,
										 0.0f, 0.0f, };
	GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

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

	//передаем в шейдерную программу атрибут текстурных координат
	glGenBuffers(1, &vboTexCoords);
	glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords); GL_CHECK_ERRORS;
	glBufferData(GL_ARRAY_BUFFER, texcoords_vec.size() * sizeof(GLfloat), &texcoords_vec[0], GL_STATIC_DRAW); GL_CHECK_ERRORS;
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (nullptr)); GL_CHECK_ERRORS;
	glEnableVertexAttribArray(2); GL_CHECK_ERRORS;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);

	return indices.size();
}

int initGL()
{
	int res = 0;

	//грузим функции opengl через glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize OpenGL context" << std::endl;
		return -1;
	}

	//выводим в консоль некоторую информацию о драйвере и контексте opengl
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

	std::cout << "Controls: " << std::endl;
	std::cout << "press right mouse button to capture/release mouse cursor  " << std::endl;
	std::cout << "press spacebar to alternate between shaded wireframe and fill display modes" << std::endl;
	std::cout << "press ESC to exit" << std::endl;

	return 0;
}

int main(int argc, char** argv)
{
	if (!glfwInit())
		return -1;

	//запрашиваем контекст opengl версии 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);


	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL basic sample", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	//регистрируем коллбеки для обработки сообщений от пользователя - клавиатура, мышь..
	glfwSetKeyCallback(window, OnKeyboardPressed);
	glfwSetCursorPosCallback(window, OnMouseMove);
	glfwSetMouseButtonCallback(window, OnMouseButtonClicked);
	glfwSetScrollCallback(window, OnMouseScroll);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (initGL() != 0)
		return -1;

	//Reset any OpenGL errors which could be present for some reason
	GLenum gl_error = glGetError();
	while (gl_error != GL_NO_ERROR)
		gl_error = glGetError();

	//создание шейдерной программы из двух файлов с исходниками шейдеров
	//используется класс-обертка ShaderProgram
	std::unordered_map<GLenum, std::string> shaders;
	shaders[GL_VERTEX_SHADER] = "../shaders/vertex.glsl";
	shaders[GL_FRAGMENT_SHADER] = "../shaders/fragment.glsl";
	ShaderProgram program(shaders); GL_CHECK_ERRORS;


	//Загружаем текстуры с помощью простого класса-обертки
	Texture2D tex1("../wall.jpg");
	Texture2D floor("../textures/onyx.jpg");
	
	//Texture2D tex4("../3.jpg");
	Texture2D tex5("../skybox2.jpg");
	
	Texture2D sun("../textures/sun.jpg");
	Texture2D venus("../textures/venus.jpg");
	Texture2D mercury("../textures/mercury.jpg");
	Texture2D mars("../textures/mars.jpg");
	Texture2D jupiter("../textures/jupiter.jpg");
	Texture2D earth("../textures/earth.jpg");
	Texture2D uranus("../textures/uranus.jpg");
	Texture2D neptune("../textures/neptune.jpg");
	Texture2D saturn("../textures/saturn.jpg");

	Texture2D mramor("../mramor.jpg");
	Texture2D str("../str.jpg");
	Texture2D tree("../tree.jpg");

	//Создаем и загружаем геометрию поверхности
	GLuint vaoTorch1, vaoTorch2;
	GLsizei Torch1 = CreateCylinder(0.6f, 5.0f, 7, vaoTorch1);
	GLsizei Torch2 = CreateCone(2.0f, 3.0f, 7, vaoTorch2);


	GLuint vaoTriStrip;
	int triStripIndices = createTriStrip(100, 100, 40, vaoTriStrip);

	GLuint vaoCube;
	GLsizei cubeIndices = createCube(vaoCube);

	GLuint vaoSquarePlane;
	GLsizei squarePlaneIndices = CreateSquarePlane(vaoSquarePlane);

	GLuint vaoCube2;
	GLsizei cubeIndices2 = CreateCube2(vaoCube2);

	GLuint vaoSolarSistem;
	GLsizei SolarSistem = CreateSphere(1.0f, 50, vaoSolarSistem);

	GLuint vaoForest;
	GLsizei Forest = CreateCone(1.0f, 1.0f, 30, vaoForest);


	GLuint vaoPlaneta1;
	GLsizei Planeta1 = CreateSphere(2.7f, 12, vaoPlaneta1);


	glViewport(0, 0, WIDTH, HEIGHT);  GL_CHECK_ERRORS;
	glEnable(GL_DEPTH_TEST);  GL_CHECK_ERRORS;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.8f, 0.8f, 0.8f, 1.0f); GL_CHECK_ERRORS;

	float d_alpha = 0.0f;
	float d_tetha = 0.0f;
	float3 triPos = make_float3(10.0f, 8.0f, 0.0f);

	float timer = 0.0f;
	//цикл обработки сообщений и отрисовки сцены каждый кадр
	while (!glfwWindowShouldClose(window))
	{
		//считаем сколько времени прошло за кадр
		GLfloat currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		glfwPollEvents();
		doCameraMovement(camera, deltaTime);

		//очищаем экран каждый кадр
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); GL_CHECK_ERRORS;

		program.StartUseShader(); GL_CHECK_ERRORS;

		//обновляем матрицы камеры и проекции каждый кадр
		float4x4 view = camera.GetViewMatrix();
		float4x4 projection = projectionMatrixTransposed(camera.zoom, float(WIDTH) / float(HEIGHT), 0.1f, 1000.0f);

		//модельная матрица, определяющая положение объекта в мировом пространстве
		float4x4 model; //начинаем с единичной матрицы

		// инстансинг
		float3 translations[100];
		int index = 0;
		for (int y = -10; y < 10; y += 2)
		{
			for (int x = -10; x < 10; x += 2)
			{
				float3 translation;
				translation = float3(y * (-y) / 5.0f, x * (-y) * y / 25.0f, sin(timer * x) * 50);
				translations[index++] = translation;
			}
		}

		

		//загружаем uniform-переменные в шейдерную программу (одинаковые для всех параллельно запускаемых копий шейдера)
		program.SetUniform("view", view);       GL_CHECK_ERRORS;
		program.SetUniform("projection", projection); GL_CHECK_ERRORS;
		program.SetUniform("model", model);
		program.SetUniform("type", 2);
		program.SetUniform("camPos", camera.pos);
		//program.SetUniform("offsets", translations);

		float xtimer = sin(timer*0.5)*5;
		float ytimer = cos(timer*0.5)*5;

		// движущийся источник света
		//program.SetUniform("pointLights[0].position", make_float3(0.0f, 0.32f, 2.03f));
		program.SetUniform("pointLights[0].position", make_float3(ytimer*3, -14.0f, xtimer));
		program.SetUniform("pointLights[0].color", make_float3(1.0f, 1.0f, 1.0f));
		program.SetUniform("pointLights[0].ambient", make_float3(0.6f, 0.6f, 0.6f));
		program.SetUniform("pointLights[0].specular", make_float3(1.0f, 1.0f, 1.0f));
		program.SetUniform("pointLights[0].mult", 2.4f);

		

	


		program.SetUniform("pointLights[1].position", make_float3(xtimer, -14.0f, ytimer+15.0f));
		program.SetUniform("pointLights[1].color", make_float3(1.0f, 1.0f, 1.0f));
		program.SetUniform("pointLights[1].ambient", make_float3(2.6f, 2.6f, 2.6f));
		program.SetUniform("pointLights[1].specular", make_float3(1.0f, 1.0f, 1.0f));
		program.SetUniform("pointLights[1].mult", 4.2f);




		//направленное освещение
		program.SetUniform("directedLights[0].direction", make_float3(0.0f, 0.0f, 0.0f));
		program.SetUniform("directedLights[0].color", make_float3(0.8f, 0.8f, 0.8f));
		program.SetUniform("directedLights[0].ambient", make_float3(0.6f, 0.6f, 0.6f));
		program.SetUniform("directedLights[0].specular", make_float3(1.0f, 1.0f, 1.0f));
		program.SetUniform("directedLights[0].mult",1.0f);


		

		program.SetUniform("material.diffuse_color", make_float3(1.0, 1.0, 1.0));
		program.SetUniform("material.speculare_color", make_float3(0.7, 0.7, 0.7));
		program.SetUniform("material.shininess", 0.7f);

		program.SetUniform("typeOfTexture", false);


		glBindVertexArray(vaoTorch1); GL_CHECK_ERRORS;
		{
			bindTexture(program, 0, "material.texture_diffuse", mramor);
			//model = transpose(mul(mul(translate4x4(float3(0.0f, 15.0f, 0.0f)), mul(rotate_Y_4x4((timer * 50) * LiteMath::DEG_TO_RAD), translate4x4(float3(15.0f, 0.0f, 0.0f)))), rotate_Z_4x4((timer * 10) * LiteMath::DEG_TO_RAD)));
			model = transpose(mul(mul(translate4x4(float3(0.0f, 20.0f, 0.0f)), translate4x4(float3(ytimer*3, -15.0f, xtimer ))), rotate_X_4x4((90) * LiteMath::DEG_TO_RAD)));
			program.SetUniform("model", model); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, Planeta1, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		}
		glBindVertexArray(0); GL_CHECK_ERRORS;
		glBindVertexArray(vaoTorch2); GL_CHECK_ERRORS;
		{
			bindTexture(program, 0, "material.texture_diffuse", mramor);
			model = transpose(mul(translate4x4(float3(0.0f, 15.0f, 0.0f)), translate4x4(float3(ytimer*3, -15.0f, xtimer))));
			program.SetUniform("model", model); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, Torch2, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		}
		glBindVertexArray(0); GL_CHECK_ERRORS;

		glBindVertexArray(vaoTorch1); GL_CHECK_ERRORS;
		{
			bindTexture(program, 0, "material.texture_diffuse", str);
			//model = transpose(mul(mul(translate4x4(float3(0.0f, 15.0f, 0.0f)), mul(rotate_Y_4x4((timer * 50) * LiteMath::DEG_TO_RAD), translate4x4(float3(15.0f, 0.0f, 0.0f)))), rotate_Z_4x4((timer * 10) * LiteMath::DEG_TO_RAD)));
			model = transpose(mul(mul(translate4x4(float3(0.0f, 20.0f, 0.0f)), translate4x4(float3(xtimer, -15.0f, ytimer+15.0f ))), rotate_X_4x4((90) * LiteMath::DEG_TO_RAD)));
			program.SetUniform("model", model); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, Planeta1, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		}
		glBindVertexArray(0); GL_CHECK_ERRORS;
		glBindVertexArray(vaoTorch2); GL_CHECK_ERRORS;
		{
			bindTexture(program, 0, "material.texture_diffuse", str);
			model = transpose(mul(translate4x4(float3(0.0f, 15.0f, 0.0f)), translate4x4(float3(xtimer, -15.0f, ytimer+15.0f ))));
			program.SetUniform("model", model); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, Torch2, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		}
		glBindVertexArray(0); GL_CHECK_ERRORS;

		//glBindVertexArray(vaoSquarePlane); GL_CHECK_ERRORS;
		//{
		//	bindTexture(program, 0, "material.texture_diffuse", floor);
		//	float4x4 model;
		//	float4x4 timeModel = mul(rotate_Y_4x4(90.0f * LiteMath::DEG_TO_RAD), translate4x4(float3(0.0f, -15.0f, 0.0f)));
		//	model = transpose(mul(timeModel, scale4x4(float3(70.0f, 1.0f, 70.0f)))); // пропорции
		//	program.SetUniform("model", model); GL_CHECK_ERRORS;
		//	glDrawElements(GL_TRIANGLES, squarePlaneIndices, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		//}
		//glBindVertexArray(0); GL_CHECK_ERRORS;

		///////////////////     Панельки
		glBindVertexArray(vaoSquarePlane);
		{

			for (int i = 1; i < 51; i++)
			{
				for (int j = 1; j < 51; j++)
				{
					bindTexture(program, 0, "material.texture_diffuse", floor);
					float4x4 model = transpose(mul(translate4x4(float3((-20.5 + i) * 1.0f, -15.001f, (-20.5 + j) * 1.0f)), scale4x4(float3((1 - 0.01 * i) * 1.0f, 1.0f, (1 - 0.01 * j) * 1.0f))));
					program.SetUniform("model", model); GL_CHECK_ERRORS;
					glDrawElements(GL_TRIANGLE_STRIP, squarePlaneIndices, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
				}
			}
		}



		glBindVertexArray(vaoCube2); GL_CHECK_ERRORS;
		{

			bindTexture(program, 0, "material.texture_diffuse", tex5);
			// небо
			float4x4 timeModel = mul(rotate_X_4x4(90.0f * LiteMath::DEG_TO_RAD), translate4x4(float3(camera.pos.x, camera.pos.z, -camera.pos.y)));
			model = transpose(mul(timeModel, scale4x4(float3(70.0f, 70.0f, 70.0f)))); // пропорции
			program.SetUniform("model", model); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLES, cubeIndices2, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		}
		glBindVertexArray(0); GL_CHECK_ERRORS;




		glBindVertexArray(vaoSolarSistem);
		{
			float h = 30.0f;


			bindTexture(program, 0, "material.texture_diffuse", sun);
			float4x4 modelSun;
			//modelSun = transpose(mul(translate4x4(float3(0.0f, h, 0.0f)),scale4x4(float3(7.55f,7.55f,7.55f))));
			modelSun = transpose(mul(rotate_Y_4x4(timer * 14.85f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 0.0f)), scale4x4(float3(7.55f, 7.55f, 7.55f)))));
			program.SetUniform("model", modelSun); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;

			bindTexture(program, 0, "material.texture_diffuse", mercury);
			float4x4 modelMercury;
			modelMercury = transpose(mul(rotate_Y_4x4(timer * 14.85f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 12.0f)), scale4x4(float3(0.04f, 0.04f, 0.04f)))));
			program.SetUniform("model", modelMercury); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;

			bindTexture(program, 0, "material.texture_diffuse", venus);
			float4x4 modelVenus;
			modelVenus = transpose(mul(rotate_Y_4x4(timer * 29.61f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 14.0f)), scale4x4(float3(0.1f, 0.1f, 0.1f)))));
			program.SetUniform("model", modelVenus); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;


			bindTexture(program, 0, "material.texture_diffuse", earth);
			float4x4 modelEarth;
			modelEarth = transpose(mul(rotate_Y_4x4(timer * 27.63f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 16.0f)), scale4x4(float3(0.107f, 0.107f, 0.107f)))));
			program.SetUniform("model", modelEarth); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
			
			bindTexture(program, 0, "material.texture_diffuse", mars);
			float4x4 modelMars;
			modelMars = transpose(mul(rotate_Y_4x4(timer * 23.85f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 18.0f)), scale4x4(float3(0.053f, 0.053f, 0.053f)))));
			program.SetUniform("model", modelMars); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;

			bindTexture(program, 0, "material.texture_diffuse", jupiter);
			float4x4 modelJupiter;
			modelJupiter = transpose(mul(rotate_Y_4x4(timer * 12.15f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 22.0f)), scale4x4(float3(1.187f, 1.187f, 1.187f)))));
			program.SetUniform("model", modelJupiter); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;

			bindTexture(program, 0, "material.texture_diffuse", saturn);
			float4x4 modelSaturn;
			modelSaturn = transpose(mul(rotate_Y_4x4(timer * 9.0f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 24.0f)), scale4x4(float3(1.0f, 1.0f, 1.0f)))));
			program.SetUniform("model", modelSaturn); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;

			bindTexture(program, 0, "material.texture_diffuse", uranus);
			float4x4 modelUranus;
			modelUranus = transpose(mul(rotate_Y_4x4(timer * 6.48f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 26.0f)), scale4x4(float3(0.42f, 0.42f, 0.42f)))));
			program.SetUniform("model", modelUranus); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;

			bindTexture(program, 0, "material.texture_diffuse", neptune);
			float4x4 modelNeptune;
			modelNeptune = transpose(mul(rotate_Y_4x4(timer * 5.13f * LiteMath::DEG_TO_RAD), mul(translate4x4(float3(0.0f, h, 28.0f)), scale4x4(float3(0.41f, 0.41f, 0.41f)))));
			program.SetUniform("model", modelNeptune); GL_CHECK_ERRORS;
			glDrawElements(GL_TRIANGLE_STRIP, SolarSistem, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
		}
		

		glBindVertexArray(vaoForest);
		{
			for (int i = 1; i < 700; i++)
			{
				bindTexture(program, 0, "material.texture_diffuse", tree);
				float4x4 model1 = transpose(mul(translate4x4(float3(sin(20 * i) * 24.0f+5.0f, -15.0f, cos(i) * 24.0f+5.0f)), scale4x4(float3(0.5f, (1.5 + sin(i / 2) / 3 * pow((-1), i)) * 1.0f, 0.5f))));
				program.SetUniform("model", model1); GL_CHECK_ERRORS;
				glDrawElements(GL_TRIANGLE_STRIP, Forest, GL_UNSIGNED_INT, nullptr); GL_CHECK_ERRORS;
			}
		}
		//----------------------------------------

		
		program.SetUniform("typeOfTexture", true);


		program.SetUniform("type", 1);


		timer += 0.1;

		glBindVertexArray(0); GL_CHECK_ERRORS;

		program.StopUseShader();

		glfwSwapBuffers(window);


	}

	//очищаем vao перед закрытием программы
	glDeleteVertexArrays(1, &vaoTriStrip);

	glfwTerminate();
	return 0;
}

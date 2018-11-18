#include "app.h"

#include <windows.h>
#include <vector>

#include "fluid.h"

App::App() {
	camera = new Camera();
	
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	window = glfwCreateWindow(width, height, "Real Time Fluid Sim", NULL, NULL);
	glfwMakeContextCurrent(window);

	initGL();

	initSim();
}

void App::runSim() {
	void *vbo_dptr = NULL;

	cudaGraphicsResource_t resource = 0;
	cudaGraphicsGLRegisterBuffer(&resource, VBO, cudaGraphicsRegisterFlagsNone);
	cudaGraphicsMapResources(1, &resource, NULL);
	size_t size;
	cudaGraphicsResourceGetMappedPointer(&vbo_dptr, &size, resource);

	iterateSim();
	fillVBOsWithMarkerParticles(vbo_dptr);

	cudaGraphicsUnmapResources(1, &resource, NULL);
	cudaGraphicsUnregisterResource(resource);
}

void App::initGL() {
	glewInit();

	glEnable(GL_PROGRAM_POINT_SIZE_EXT);
	glPointSize(5);

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	std::string vertexShaderFilename = std::string("../src/shaders/basic.vertex.glsl");
	std::string vertexShaderSourceString = readFileAsString(vertexShaderFilename);
	const char *vertexShaderSource = vertexShaderSourceString.c_str();

	std::string fragmentShaderFilename = std::string("../src/shaders/basic.frag.glsl");
	std::string fragmentShaderSourceString = readFileAsString(fragmentShaderFilename);
	const char *fragmentShaderSource = fragmentShaderSourceString.c_str();

	// Compile the vertex shader
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	// Compile the fragment shader
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// check for shader compile errors
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	// Link the shaders
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	std::vector<int> indices;
	for (int i = 0; i < NUM_MARKER_PARTICLES; ++i) {
		indices.push_back(i);
	}

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glGenBuffers(1, &PBO);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(GLubyte) * width * height * 4, NULL, GL_DYNAMIC_COPY);
	cudaGLRegisterBufferObject(PBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &*indices.begin(), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * NUM_MARKER_PARTICLES, NULL, GL_STATIC_DRAW);

	GLuint vsPosLocation = glGetAttribLocation(shaderProgram, "vs_Pos");
	glEnableVertexAttribArray(vsPosLocation);
	glVertexAttribPointer(vsPosLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	
	GLuint vsColorLocation = glGetAttribLocation(shaderProgram, "vs_Color");
	glEnableVertexAttribArray(vsColorLocation);
	glVertexAttribPointer(vsColorLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (GLvoid*)(3 * sizeof(float)));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void App::start() {
	while (!glfwWindowShouldClose(window)) {
		runSim();

		glfwPollEvents();

		P = glm::frustum<float>(-camera->scale * ((float)width) / ((float)height),
			camera->scale * ((float)width / (float)height),
			-camera->scale, camera->scale, 1.0, 1000.0);

		M = glm::mat4();

		V = 
			glm::translate(glm::mat4(), glm::vec3(camera->x_trans, camera->y_trans, camera->z_trans))
			* glm::rotate(glm::mat4(), camera->x_angle, glm::vec3(1.0f, 0.0f, 0.0f))
			* glm::rotate(glm::mat4(), camera->y_angle, glm::vec3(0.0f, 1.0f, 0.0f));

		glm::mat3 MV_normal = glm::transpose(glm::inverse(glm::mat3(V) * glm::mat3(M)));
		glm::mat4 MV = V * M;
		MVP = P * MV;

		draw();
	}

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);

	cudaGLUnregisterBufferObject(PBO);
	glBindBuffer(GL_ARRAY_BUFFER, PBO);
	glDeleteBuffers(1, &PBO);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void App::draw() {
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	uchar4 *pbo_dptr = NULL;
	cudaGLMapBufferObject((void**)&pbo_dptr, PBO);

	raymarchPBO(pbo_dptr, glm::vec3(camera->x_trans, camera->y_trans, camera->z_trans), (float)width, (float)height);

	cudaGLUnmapBufferObject(PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);

	glUseProgram(shaderProgram);

	GLuint uMVP = glGetUniformLocation(shaderProgram, "u_MVP");
	glUniformMatrix4fv(uMVP, 1, GL_FALSE, &MVP[0][0]);

	//glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//glBindVertexArray(VAO);
	//glDrawElements(GL_POINTS, NUM_MARKER_PARTICLES, GL_UNSIGNED_INT, 0);

	glfwSwapBuffers(window);
}

std::string App::readFileAsString(std::string filename) {
	std::string fileString;
	std::ifstream fileStream(filename);
	if (fileStream.is_open()) {
		std::stringstream sstr;
		sstr << fileStream.rdbuf();
		fileString = sstr.str();
		fileStream.close();
		return fileString;
	}
	else {
		std::cout << "file does not exist" << std::endl;
		return "";
	}
}

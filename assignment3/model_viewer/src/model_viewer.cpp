// Assignment 3, Part 1 and 2
//
// Modify this file according to the lab instructions.
//

#include "utils.h"
#include "utils2.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef WITH_TWEAKBAR
#include <AntTweakBar.h>
#endif // WITH_TWEAKBAR

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cstdlib>
#include <algorithm>

#define NUM_CUBEMAP_LEVELS 8

// The attribute locations we will use in the vertex shader
enum AttributeLocation {
    POSITION = 0,
    NORMAL = 1
};

enum LensType {
	ORTOGRAPHIC = 0,
	PERSPECTIVE = 1
};

enum ColorMode {
	NORMAL_AS_RGB = 0,
	BLINN_PHONG = 1,
	REFLECTION = 2,
	SIZE
};

struct Camera {
	glm::vec3 position;
	glm::quat orientation;
	LensType lensType;
	float zoom;
	float yaw, pitch;
};

// Struct for representing an indexed triangle mesh
struct Mesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
};

// Struct for representing a vertex array object (VAO) created from a
// mesh. Used for rendering.
struct MeshVAO {
    GLuint vao;
    GLuint vertexVBO;
    GLuint normalVBO;
    GLuint indexVBO;
    int numVertices;
    int numIndices;
};

struct SkyboxVAO {
	GLuint vao;
	GLuint vertexVBO;
	GLuint indexVBO;
	int numVertices;
	int numIndices;
};

// Struct for resources and state
struct Context {
    int width;
    int height;
    float aspect;
	float zoom;

	LensType lensType;

    GLFWwindow *window;
    GLuint program;
	GLuint skyboxProgram;

    Trackball trackball;
	
	GLuint defaultVAO;

    Mesh mesh;
    MeshVAO meshVAO;

	SkyboxVAO skyboxVAO;
    
	GLuint cubemap;
	GLuint cubemap_prefiltered_levels[NUM_CUBEMAP_LEVELS];
	GLuint cubemap_prefiltered_mipmap;
	int cubemap_index;

	glm::vec3 background_color;

	glm::vec3 ambient_light;

	glm::vec3 light_position;
	glm::vec3 light_color;

	glm::vec3 diffuse_color;
	glm::vec3 specular_color;
	GLfloat specular_power;

	float ambient_weight;
	float diffuse_weight;
	float specular_weight;

	ColorMode color_mode;
	int use_gamma_correction;
	int use_color_inversion;
	
    float elapsed_time;
};

// Returns the value of an environment variable
std::string getEnvVar(const std::string &name)
{
    char const* value = std::getenv(name.c_str());
    if (value == nullptr) {
        return std::string();
    }
    else {
        return std::string(value);
    }
}

// Returns the absolute path to the shader directory
std::string shaderDir(void)
{
    std::string rootDir = getEnvVar("ASSIGNMENT3_ROOT");
    if (rootDir.empty()) {
        std::cout << "Error: ASSIGNMENT3_ROOT is not set." << std::endl;
		std::getchar();
        std::exit(EXIT_FAILURE);
    }
    return rootDir + "/model_viewer/src/shaders/";
}

// Returns the absolute path to the 3D model directory
std::string modelDir(void)
{
    std::string rootDir = getEnvVar("ASSIGNMENT3_ROOT");
    if (rootDir.empty()) {
        std::cout << "Error: ASSIGNMENT3_ROOT is not set." << std::endl;
		std::getchar();
        std::exit(EXIT_FAILURE);
    }
    return rootDir + "/model_viewer/3d_models/";
}

// Returns the absolute path to the cubemap texture directory
std::string cubemapDir(void)
{
    std::string rootDir = getEnvVar("ASSIGNMENT3_ROOT");
    if (rootDir.empty()) {
        std::cout << "Error: ASSIGNMENT3_ROOT is not set." << std::endl;
		std::getchar();
        std::exit(EXIT_FAILURE);
    }
    return rootDir + "/model_viewer/cubemaps/";
}

void loadMesh(const std::string &filename, Mesh *mesh)
{
    OBJMesh obj_mesh;
    objMeshLoad(obj_mesh, filename);
    mesh->vertices = obj_mesh.vertices;
    mesh->normals = obj_mesh.normals;
    mesh->indices = obj_mesh.indices;
}

void createMeshVAO(Context &ctx, const Mesh &mesh, MeshVAO *meshVAO)
{
    // Generates and populates a VBO for the vertices
    glGenBuffers(1, &(meshVAO->vertexVBO));
    glBindBuffer(GL_ARRAY_BUFFER, meshVAO->vertexVBO);
    auto verticesNBytes = mesh.vertices.size() * sizeof(mesh.vertices[0]);
    glBufferData(GL_ARRAY_BUFFER, verticesNBytes, mesh.vertices.data(), GL_STATIC_DRAW);

    // Generates and populates a VBO for the vertex normals
    glGenBuffers(1, &(meshVAO->normalVBO));
    glBindBuffer(GL_ARRAY_BUFFER, meshVAO->normalVBO);
    auto normalsNBytes = mesh.normals.size() * sizeof(mesh.normals[0]);
    glBufferData(GL_ARRAY_BUFFER, normalsNBytes, mesh.normals.data(), GL_STATIC_DRAW);

    // Generates and populates a VBO for the element indices
    glGenBuffers(1, &(meshVAO->indexVBO));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshVAO->indexVBO);
    auto indicesNBytes = mesh.indices.size() * sizeof(mesh.indices[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesNBytes, mesh.indices.data(), GL_STATIC_DRAW);

    // Creates a vertex array object (VAO) for drawing the mesh
    glGenVertexArrays(1, &(meshVAO->vao));
    glBindVertexArray(meshVAO->vao);
    glBindBuffer(GL_ARRAY_BUFFER, meshVAO->vertexVBO);
    glEnableVertexAttribArray(POSITION);
    glVertexAttribPointer(POSITION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, meshVAO->normalVBO);
    glEnableVertexAttribArray(NORMAL);
    glVertexAttribPointer(NORMAL, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshVAO->indexVBO);
    glBindVertexArray(ctx.defaultVAO); // unbinds the VAO

    // Additional information required by draw calls
    meshVAO->numVertices = mesh.vertices.size();
    meshVAO->numIndices = mesh.indices.size();
}

void createSkyboxVAO(Context &ctx, SkyboxVAO *skyboxVAO)
{
	// Generates and populates a VBO for the vertices
    glGenBuffers(1, &(skyboxVAO->vertexVBO));
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVAO->vertexVBO);
	const GLfloat vertexPositions[] = {
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
	};
	auto verticesNBytes = sizeof(vertexPositions);
	glBufferData(GL_ARRAY_BUFFER, verticesNBytes, vertexPositions, GL_STATIC_DRAW);

	// Generates and populates a VBO for the element indices
    glGenBuffers(1, &(skyboxVAO->indexVBO));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxVAO->indexVBO);
	const GLubyte indices[] = {
		0,1,2,
		2,3,0,
	};
	auto indicesNBytes = sizeof(indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesNBytes, indices, GL_STATIC_DRAW);

	// Creates a vertex array object (VAO) for drawing the mesh
    glGenVertexArrays(1, &(skyboxVAO->vao));
    glBindVertexArray(skyboxVAO->vao);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVAO->vertexVBO);
    glEnableVertexAttribArray(POSITION);
    glVertexAttribPointer(POSITION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxVAO->indexVBO);
    glBindVertexArray(ctx.defaultVAO); // unbinds the VAO

    // Additional information required by draw calls
	skyboxVAO->numVertices = sizeof(vertexPositions) / sizeof(vertexPositions[0]);
	skyboxVAO->numIndices = sizeof(indices) / sizeof(indices[0]);
}

void initializeTrackball(Context &ctx)
{
    double radius = double(std::min(ctx.width, ctx.height)) / 2.0;
    ctx.trackball.radius = radius;
    glm::vec2 center = glm::vec2(ctx.width, ctx.height) / 2.0f;
    ctx.trackball.center = center;
}

void init(Context &ctx)
{
    ctx.program = loadShaderProgram(shaderDir() + "mesh.vert", shaderDir() + "mesh.frag");

	ctx.skyboxProgram = loadShaderProgram(shaderDir() + "skybox.vert", shaderDir() + "skybox.frag");

    loadMesh((modelDir() + "gargo.obj"), &ctx.mesh);
    createMeshVAO(ctx, ctx.mesh, &ctx.meshVAO);

	createSkyboxVAO(ctx, &ctx.skyboxVAO);

    // Load cubemap texture(s)
    // ...
	const std::string cubemap_path = cubemapDir() + "/Forrest/";
	ctx.cubemap = loadCubemap(cubemap_path);
	const std::string levels[] = { "2048", "512", "128", "32", "8", "2", "0.5", "0.125" };
	for (int i=0; i < NUM_CUBEMAP_LEVELS; i++) {
		ctx.cubemap_prefiltered_levels[i] = loadCubemap(cubemap_path + "prefiltered/" + levels[i]);
	}
	//ctx.cubemap_prefiltered_mipmap = loadCubemapMipmap(cubemap_path + "prefiltered/");
	ctx.cubemap_index = 0;

	ctx.zoom = 1.0f;
	ctx.lensType = LensType::PERSPECTIVE;

    initializeTrackball(ctx);

	ctx.background_color = glm::vec3(0.2f);

	ctx.ambient_light = glm::vec3(0.04f);

	ctx.light_position = glm::vec3(1.0, 1.0, 1.0);
	ctx.light_color = glm::vec3(1.0, 1.0, 1.0);

	ctx.diffuse_color = glm::vec3(0.1, 1.0, 0.1);
	ctx.specular_color = glm::vec3(0.04);
	ctx.specular_power = 60.0f;

	ctx.ambient_weight = 1.0f;
	ctx.diffuse_weight = 1.0f;
	ctx.specular_weight = 1.0f;

	ctx.color_mode = ColorMode::NORMAL_AS_RGB;
	ctx.use_gamma_correction = 1;
	ctx.use_color_inversion = 0;
}

void getViewMatrix(glm::mat4 *dst)
{
	*dst = glm::lookAt(glm::vec3(0, 0, 2), glm::vec3(), glm::vec3(0, 1, 0));
}

float getFovy(Context &ctx) 
{
	if (ctx.lensType == LensType::PERSPECTIVE) {
		return 2.0f / pow(2.0f, ctx.zoom);
	}
	return 0;
}

void drawSkybox(Context &ctx)
{
	glm::mat4 view;
	getViewMatrix(&view);
	glm::mat4 view_transpose = glm::transpose(view);

	float fovy = getFovy(ctx);
	float aspect = ctx.aspect;

    // Activate program
	glUseProgram(ctx.skyboxProgram);

    // Bind textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ctx.cubemap);
	glUniform1i(glGetUniformLocation(ctx.skyboxProgram, "u_cubemap"), /*GL_TEXTURE0*/ 0);

	glUniformMatrix4fv(glGetUniformLocation(ctx.skyboxProgram, "u_view_transpose"), 1, GL_FALSE, &view_transpose[0][0]);
	glUniform1f(glGetUniformLocation(ctx.skyboxProgram, "u_fovy"), fovy);
	glUniform1f(glGetUniformLocation(ctx.skyboxProgram, "u_aspect"), aspect);

	// Draw!
	glBindVertexArray(ctx.skyboxVAO.vao);
	glDrawElements(GL_TRIANGLES, ctx.skyboxVAO.numIndices, GL_UNSIGNED_BYTE, 0);
    glBindVertexArray(ctx.defaultVAO);
}

// MODIFY THIS FUNCTION
void drawMesh(Context &ctx, GLuint program, const MeshVAO &meshVAO)
{
    // Define uniforms
    glm::mat4 model = trackballGetRotationMatrix(ctx.trackball);

	glm::mat4 view;
	getViewMatrix(&view);
	
	float zNear = 0.1f;
	float zFar = 100.f;
	glm::mat4 projection;
	if (ctx.lensType == LensType::PERSPECTIVE) {
		float fovy = getFovy(ctx);
		projection = glm::perspective(fovy, ctx.aspect, zNear, zFar);
	}
	else {
		float hh = 2.0f / pow(2.0f, ctx.zoom);
		projection = glm::ortho(-hh * ctx.aspect, hh * ctx.aspect, -hh, hh, zNear, zFar);
	}

    glm::mat4 mv = view * model;
    glm::mat4 mvp = projection * mv;

    // Activate program
    glUseProgram(program);

    // Bind textures
    // ...
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ctx.cubemap_prefiltered_levels[ctx.cubemap_index]);
	//glBindTexture(GL_TEXTURE_CUBE_MAP, ctx.cubemap_prefiltered_mipmap);
	glUniform1i(glGetUniformLocation(program, "u_cubemap"), /*GL_TEXTURE0*/ 0);
	//glUniform1f(glGetUniformLocation(program, "u_cubemap_lod"), (float)ctx.cubemap_index);
	//glBindTexture(GL_TEXTURE_CUBE_MAP, 0);


    // Pass uniforms
	glUniformMatrix4fv(glGetUniformLocation(program, "u_v"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "u_mv"), 1, GL_FALSE, &mv[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "u_mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniform1f(glGetUniformLocation(program, "u_time"), ctx.elapsed_time);
    // ...
	glUniform3fv(glGetUniformLocation(program, "u_ambient_light"), 1, &ctx.ambient_light[0]);

	glUniform3fv(glGetUniformLocation(program, "u_light_position"), 1, &ctx.light_position[0]);
	glUniform3fv(glGetUniformLocation(program, "u_light_color"), 1, &ctx.light_color[0]);
	
	glUniform3fv(glGetUniformLocation(program, "u_diffuse_color"), 1, &ctx.diffuse_color[0]);
	glUniform3fv(glGetUniformLocation(program, "u_specular_color"), 1, &ctx.specular_color[0]);
	glUniform1f(glGetUniformLocation(program, "u_specular_power"), ctx.specular_power);

	glUniform1f(glGetUniformLocation(program, "u_ambient_weight"), ctx.ambient_weight);
	glUniform1f(glGetUniformLocation(program, "u_diffuse_weight"), ctx.diffuse_weight);
	glUniform1f(glGetUniformLocation(program, "u_specular_weight"), ctx.specular_weight);

	glUniform1i(glGetUniformLocation(program, "u_color_mode"), ctx.color_mode);
	glUniform1i(glGetUniformLocation(program, "u_use_gamma_correction"), ctx.use_gamma_correction);
	glUniform1i(glGetUniformLocation(program, "u_use_color_inversion"), ctx.use_color_inversion);

    // Draw!
    glBindVertexArray(meshVAO.vao);
    glDrawElements(GL_TRIANGLES, meshVAO.numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(ctx.defaultVAO);
}

void display(Context &ctx)
{
    glClearColor(ctx.background_color[0], ctx.background_color[1], ctx.background_color[2], 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	drawSkybox(ctx);
	glDepthMask(GL_TRUE);

    glEnable(GL_DEPTH_TEST); // ensures that polygons overlap correctly
    drawMesh(ctx, ctx.program, ctx.meshVAO);
}

void reloadShaders(Context *ctx)
{
    glDeleteProgram(ctx->program);
    ctx->program = loadShaderProgram(shaderDir() + "mesh.vert", shaderDir() + "mesh.frag");
	ctx->skyboxProgram = loadShaderProgram(shaderDir() + "skybox.vert", shaderDir() + "skybox.frag");
}

void mouseButtonPressed(Context *ctx, int button, int x, int y)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        ctx->trackball.center = glm::vec2(x, y);
        trackballStartTracking(ctx->trackball, glm::vec2(x, y));
    }
}

void mouseButtonReleased(Context *ctx, int button, int x, int y)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        trackballStopTracking(ctx->trackball);
    }
}

void moveTrackball(Context *ctx, int x, int y)
{
    if (ctx->trackball.tracking) {
        trackballMove(ctx->trackball, glm::vec2(x, y));
    }
}

void errorCallback(int error, const char* description)
{
    std::cerr << description << std::endl;
}


void toggleLens(Context *ctx)
{
	ctx->lensType = ctx->lensType == ORTOGRAPHIC ? PERSPECTIVE : ORTOGRAPHIC;
}

void toggleAmbient(Context *ctx)
{
	ctx->ambient_weight = ctx->ambient_weight == 0.0f ? 1.0f : 0.0f;
}

void toggleDiffuse(Context *ctx)
{
	ctx->diffuse_weight = ctx->diffuse_weight == 0.0f ? 1.0f : 0.0f;
}

void toggleSpecular(Context *ctx)
{
	ctx->specular_weight = ctx->specular_weight == 0.0f ? 1.0f : 0.0f;
}


void toggleColoringMode(Context *ctx)
{
	ctx->color_mode = (ColorMode)((ctx->color_mode + 1) % ColorMode::SIZE);
}

void toggleGammaCorrection(Context *ctx)
{
	ctx->use_gamma_correction ^= 1;
}

void toggleColoringInversion(Context *ctx)
{
	ctx->use_color_inversion ^= 1;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
#ifdef WITH_TWEAKBAR
    if (TwEventKeyGLFW3(window, key, scancode, action, mods)) return;
#endif // WITH_TWEAKBAR

    Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
	if (action == GLFW_PRESS) {
		switch (key)
		{
		case GLFW_KEY_R:
			reloadShaders(ctx);
			break;
		case GLFW_KEY_Q:
			toggleLens(ctx);
			break;
		case GLFW_KEY_Z:
			toggleAmbient(ctx);
			break;
		case GLFW_KEY_X:
			toggleDiffuse(ctx);
			break;
		case GLFW_KEY_C:
			toggleSpecular(ctx);
			break;
		case GLFW_KEY_F:
			toggleColoringMode(ctx);
			break;
		case GLFW_KEY_G:
			toggleGammaCorrection(ctx);
			break;
		case GLFW_KEY_H:
			toggleColoringInversion(ctx);
			break;
		case GLFW_KEY_T:
			if (ctx->cubemap_index > 0)
				ctx->cubemap_index--;
			break;
		case GLFW_KEY_Y:
			if (ctx->cubemap_index < NUM_CUBEMAP_LEVELS - 1)
				ctx->cubemap_index++;
			break;
		default:
			break;
		}
	}
	/*if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        reloadShaders(ctx);
    }*/
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
#ifdef WITH_TWEAKBAR
    if (TwEventMouseButtonGLFW3(window, button, action, mods)) return;
#endif // WITH_TWEAKBAR

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
    if (action == GLFW_PRESS) {
        mouseButtonPressed(ctx, button, x, y);
    }
    else {
        mouseButtonReleased(ctx, button, x, y);
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y)
{
#ifdef WITH_TWEAKBAR
    if (TwEventCursorPosGLFW3(window, x, y)) return;
#endif // WITH_TWEAKBAR

    Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
    moveTrackball(ctx, x, y);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) 
{
	Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
	ctx->zoom += yoffset / 8.0;
	if (ctx->zoom < 0)
		ctx->zoom = 0;
	if (ctx->zoom > 4)
		ctx->zoom = 4;
}

void resizeCallback(GLFWwindow* window, int width, int height)
{
#ifdef WITH_TWEAKBAR
    TwWindowSize(width, height);
#endif // WITH_TWEAKBAR

    Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
    ctx->width = width;
    ctx->height = height;
    ctx->aspect = float(width) / float(height);
    ctx->trackball.radius = double(std::min(width, height)) / 2.0;
    ctx->trackball.center = glm::vec2(width, height) / 2.0f;
    glViewport(0, 0, width, height);
}

int main(void)
{
    Context ctx;

    // Create a GLFW window
    glfwSetErrorCallback(errorCallback);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    ctx.width = 800;
    ctx.height = 600;
    ctx.aspect = float(ctx.width) / float(ctx.height);
    ctx.window = glfwCreateWindow(ctx.width, ctx.height, "Model viewer", nullptr, nullptr);
    glfwMakeContextCurrent(ctx.window);
    glfwSetWindowUserPointer(ctx.window, &ctx);
    glfwSetKeyCallback(ctx.window, keyCallback);
    glfwSetMouseButtonCallback(ctx.window, mouseButtonCallback);
    glfwSetCursorPosCallback(ctx.window, cursorPosCallback);
	glfwSetScrollCallback(ctx.window, scrollCallback);
    glfwSetFramebufferSizeCallback(ctx.window, resizeCallback);

    // Load OpenGL functions
    glewExperimental = true;
    GLenum status = glewInit();
    if (status != GLEW_OK) {
        std::cerr << "Error: " << glewGetErrorString(status) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

    // Initialize AntTweakBar (if enabled)
#ifdef WITH_TWEAKBAR
    TwInit(TW_OPENGL_CORE, nullptr);
    TwWindowSize(ctx.width, ctx.height);
    TwBar *tweakbar = TwNewBar("Settings");
	TwDefine("Settings size='300 500'");
	TwDefine("Settings refresh=0.1");
	TwDefine("Settings valueswidth=fit");
	TwEnumVal lensEV[] = {
		{ORTOGRAPHIC, "Orthographic"}, 
		{PERSPECTIVE, "Perspective"}
	};
	TwType lensType = TwDefineEnum("LensType", lensEV, 2);
	TwAddVarRW(tweakbar, "Lens / Projection", lensType, &ctx.lensType, NULL);
	TwAddSeparator(tweakbar, NULL, NULL);
	TwAddVarRW(tweakbar, "Background color", TW_TYPE_COLOR3F, &ctx.background_color[0], "colormode=hls");
	TwAddSeparator(tweakbar, NULL, NULL);
	TwAddVarRW(tweakbar, "Ambient light color", TW_TYPE_COLOR3F, &ctx.ambient_light[0], "colormode=hls");
	TwAddVarRW(tweakbar, "Point light color", TW_TYPE_COLOR3F, &ctx.light_color[0], "colormode=hls");
	TwAddVarRW(tweakbar, "Point light position", TW_TYPE_DIR3F, &ctx.light_position[0], NULL);
	//TwAddVarRW(tweakbar, "Point light x", TW_TYPE_FLOAT, &ctx.light_position[0], NULL);
	//TwAddVarRW(tweakbar, "Point light y", TW_TYPE_FLOAT, &ctx.light_position[1], NULL);
	//TwAddVarRW(tweakbar, "Point light z", TW_TYPE_FLOAT, &ctx.light_position[2], NULL);
	TwAddSeparator(tweakbar, NULL, NULL);
	TwAddVarRW(tweakbar, "Material diffuse color", TW_TYPE_COLOR3F, &ctx.diffuse_color[0], "colormode=hls");
	TwAddVarRW(tweakbar, "Material specular color", TW_TYPE_COLOR3F, &ctx.specular_color[0], "colormode=hls");
	TwAddVarRW(tweakbar, "Material specular power", TW_TYPE_FLOAT, &ctx.specular_power, NULL);
	TwAddSeparator(tweakbar, NULL, NULL);
	TwEnumVal colorModeEV[] = {
		{NORMAL_AS_RGB, "Normal as RGB"}, 
		{BLINN_PHONG, "Blinn-Phong"}, 
		{REFLECTION, "Reflection"}
	};
	TwType colorModeType = TwDefineEnum("ColorMode", colorModeEV, ColorMode::SIZE);
	TwAddVarRW(tweakbar, "Color mode", colorModeType, &ctx.color_mode, NULL);
	TwAddVarRW(tweakbar, "Use gamma correction", TW_TYPE_BOOL32, &ctx.use_gamma_correction, NULL);
	TwAddVarRW(tweakbar, "Use color inversion", TW_TYPE_BOOL32, &ctx.use_color_inversion, NULL);
	TwAddSeparator(tweakbar, NULL, NULL);
	TwAddVarRW(tweakbar, "Ambient weight", TW_TYPE_FLOAT, &ctx.ambient_weight, NULL);
	TwAddVarRW(tweakbar, "Diffuse weight", TW_TYPE_FLOAT, &ctx.diffuse_weight, NULL);
	TwAddVarRW(tweakbar, "Specular weight", TW_TYPE_FLOAT, &ctx.specular_weight, NULL);
#endif // WITH_TWEAKBAR

    // Initialize rendering
    glGenVertexArrays(1, &ctx.defaultVAO);
    glBindVertexArray(ctx.defaultVAO);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    init(ctx);

    // Start rendering loop
    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();
        ctx.elapsed_time = glfwGetTime();
        display(ctx);
#ifdef WITH_TWEAKBAR
        TwDraw();
#endif // WITH_TWEAKBAR
        glfwSwapBuffers(ctx.window);
    }

    // Shutdown
#ifdef WITH_TWEAKBAR
    TwTerminate();
#endif // WITH_TWEAKBAR
    glfwDestroyWindow(ctx.window);
    glfwTerminate();
    std::exit(EXIT_SUCCESS);
}

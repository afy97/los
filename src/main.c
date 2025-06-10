#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 720
#define HEIGHT 720
#define WORK_GROUP_SIZE 8

static const char* shaderPath  = "assets/shader/shader.comp";
static const char* texturePath = "assets/stratis.png";

static GLuint numGroupsX = (WIDTH + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
static GLuint numGroupsY = (HEIGHT + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;
static GLuint program;

GLuint createShaderProgram(GLenum type, const char* path)
{
    FILE* file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* src = (char*)malloc(size + 1);
    fread(src, 1, size, file);
    fclose(file);
    src[size] = '\0';

    GLint  success = 0;
    GLuint shader  = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&src, (GLint*)&size);
    glCompileShader(shader);
    free(src);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        if (logLength > 0) {
            char* infoLog = (char*)malloc(logLength);
            glGetShaderInfoLog(shader, logLength, NULL, infoLog);
            fprintf(stderr, "Shader compilation error: %s\n", infoLog);
            fflush(stderr);
            free(infoLog);
        }

        glDeleteShader(shader);

        return UINT_MAX;
    }

    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        if (logLength > 0) {
            char* infoLog = (char*)malloc(logLength);
            glGetProgramInfoLog(program, logLength, NULL, infoLog);
            fprintf(stderr, "Program linking error: %s\n", infoLog);
            fflush(stderr);
            free(infoLog);
        }

        glDeleteProgram(program);
        glDeleteShader(shader);

        return UINT_MAX;
    }

    glDetachShader(program, shader);
    glDeleteShader(shader);

    return program;
}

GLuint loadTexture(const char* path)
{
    stbi_set_flip_vertically_on_load(1);

    int    w, h, c;
    float* image = stbi_loadf(path, &w, &h, &c, 0);

    GLenum format;
    switch (c) {
    case 1:
        format = GL_RED;
        break;
    case 3:
        format = GL_RGB;
        break;
    case 4:
        format = GL_RGBA;
        break;
    default:
        format = GL_INVALID_ENUM;
        break;
    }

    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, GL_RGBA32F, w, h);
    glTextureSubImage2D(texture, 0, 0, 0, w, h, format, GL_FLOAT, image);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glGenerateTextureMipmap(texture);

    stbi_image_free(image);

    return texture;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        GLuint newProgram = createShaderProgram(GL_COMPUTE_SHADER, shaderPath);

        if (newProgram != UINT_MAX) {
            glDeleteProgram(program);
            glUseProgram(newProgram);

            program = newProgram;

            printf("Shader reload\n");
            fflush(stdout);
        }
    }
}

void errorCallback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error (%d): %s\n", error, description);
    fflush(stderr);
}

int main()
{
    glfwSetErrorCallback(errorCallback);
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Window", NULL, NULL);

    glfwSetKeyCallback(window, keyCallback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewInit();

    GLuint image = loadTexture(texturePath);
    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, GL_RGBA32F, WIDTH, HEIGHT);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint framebuffer;
    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, texture, 0);

    int    initial = 0;
    GLuint outputBuffer;
    glCreateBuffers(1, &outputBuffer);
    glNamedBufferStorage(outputBuffer, sizeof(int), NULL, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    program = createShaderProgram(GL_COMPUTE_SHADER, shaderPath);
    glUseProgram(program);
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindTextureUnit(1, image);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, outputBuffer);

    int lineOfSight = 0;

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        double time = glfwGetTime();
        glUniform1f(0, (float)time);
        glUniform3f(1, 0.2f, 0.2f, (float)((sin(time) + 1) / 2));
        glUniform3f(2, 0.8f, 0.8f, (float)((sin(time) + 1) / 2));
        glNamedBufferSubData(outputBuffer, 0, sizeof(int), &initial);

        glDispatchCompute(numGroupsX, numGroupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        void* ptr = glMapNamedBuffer(outputBuffer, GL_READ_ONLY);
        int   los = (*(int*)ptr) == 0;
        glUnmapNamedBuffer(outputBuffer);

        if (los != lineOfSight) {
            printf("%s\n", los ? "LOS" : "BLOCKED");
            fflush(stdout);
            lineOfSight = los;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

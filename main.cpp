#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION
#include <cmath>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"

// Shader compilation functions
unsigned int compileShader(GLenum type, const char* path);
unsigned int createShader(const char* vsSource, const char* fsSource);
static unsigned loadImageToTexture(const char* filePath);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

float speed = 0.5f;
bool paused = false;
float cameraAngle = 0.0f;
float cameraDistance = 0.8f;  // Base camera distance
float fov = 65.0f; // initial field of view angle in degrees
float currentAngle = 0.0f;  // Current angle for train movement
unsigned int studentTexture;
bool cullingEnabled = false;  // Global variable for face culling state
float lastSpeed;
bool spacePressed = false;

void processInput(GLFWwindow* window) {
    static bool keyPressed = false;  // Only keyPressed needs to be static now

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) speed += 0.008f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) speed -= 0.008f;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !spacePressed)
    {
        spacePressed = true;
        if (!paused) {
            lastSpeed = speed;  // Spremi brzinu prije pauziranja
            speed = 0.0f;
        }
        else {
            speed = lastSpeed;  // Vrati zadnju spremljenu brzinu
        }
        paused = !paused;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        spacePressed = false;
    }
    // Vraćamo rotaciju kamere
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraAngle -= 0.02f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraAngle += 0.02f;
    if (speed < 0) speed = 0;

    // Toggle face culling with 'C' key
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!keyPressed) {
            cullingEnabled = !cullingEnabled;
            if (cullingEnabled) {
                glEnable(GL_CULL_FACE);
            }
            else {
                glDisable(GL_CULL_FACE);
            }
            keyPressed = true;
        }
    }
    else {
        keyPressed = false;
    }
}

int main()
{
    if (!glfwInit())
    {
        std::cout << "GLFW Biblioteka se nije ucitala! :(\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    unsigned int wWidth = 800;
    unsigned int wHeight = 600;
    GLFWwindow* window = glfwCreateWindow(wWidth, wHeight, "Voz sa stazom", NULL, NULL);

    if (window == NULL)
    {
        std::cout << "Prozor nije napravljen! :(\n";
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK)
    {
        std::cout << "GLEW nije mogao da se ucita! :'(\n";
        return 3;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Postavi VSync na 1 (60 FPS)
    glfwSwapInterval(1);

    // Shaderi
    unsigned int shader = createShader("basic.vert", "basic.frag");
    glUseProgram(shader);
    unsigned int studentShader = createShader("student.vert", "student.frag");
    unsigned int groundShader = createShader("ground.vert", "ground.frag");
    // Vertices for the train track (circle)
    const int trackSegments = 100;
    std::vector<float> trackVertices;
    float trackRadius = 3.0f;  // Larger radius for the track
    float cubeRadius = 1.5f;   // Original radius for cubes

    for (int i = 0; i <= trackSegments; i++) {
        float angle = glm::two_pi<float>() * i / trackSegments;
        float x = trackRadius * cos(angle);
        float z = trackRadius * sin(angle);
        // Position (x, y, z) and color (r, g, b, a)
        trackVertices.push_back(x);
        trackVertices.push_back(-0.1f);  // Slightly below ground
        trackVertices.push_back(z);
        trackVertices.push_back(0.5f);  // Gray color
        trackVertices.push_back(0.5f);
        trackVertices.push_back(0.5f);
        trackVertices.push_back(1.0f);
    }

    // Create and setup track VAO
    unsigned int trackVAO, trackVBO;
    glGenVertexArrays(1, &trackVAO);
    glGenBuffers(1, &trackVBO);

    glBindVertexArray(trackVAO);
    glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
    glBufferData(GL_ARRAY_BUFFER, trackVertices.size() * sizeof(float), trackVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Vertices za kocku (24 vertices - 4 per face)
    float vertices[] = {
        // Front face
        -0.125f, -0.125f,  0.125f,  0.0f, 0.0f,  // 0
         0.125f, -0.125f,  0.125f,  1.0f, 0.0f,  // 1
         0.125f,  0.125f,  0.125f,  1.0f, 1.0f,  // 2
        -0.125f,  0.125f,  0.125f,  0.0f, 1.0f,  // 3

        // Back face
        -0.125f, -0.125f, -0.125f,  0.0f, 0.0f,  // 4
         0.125f, -0.125f, -0.125f,  1.0f, 0.0f,  // 5
         0.125f,  0.125f, -0.125f,  1.0f, 1.0f,  // 6
        -0.125f,  0.125f, -0.125f,  0.0f, 1.0f,  // 7

        // Top face
        -0.125f,  0.125f, -0.125f,  0.0f, 0.0f,  // 8
         0.125f,  0.125f, -0.125f,  1.0f, 0.0f,  // 9
         0.125f,  0.125f,  0.125f,  1.0f, 1.0f,  // 10
        -0.125f,  0.125f,  0.125f,  0.0f, 1.0f,  // 11

        // Bottom face
        -0.125f, -0.125f, -0.125f,  0.0f, 0.0f,  // 12
         0.125f, -0.125f, -0.125f,  1.0f, 0.0f,  // 13
         0.125f, -0.125f,  0.125f,  1.0f, 1.0f,  // 14
        -0.125f, -0.125f,  0.125f,  0.0f, 1.0f,  // 15

        // Right face
         0.125f, -0.125f, -0.125f,  0.0f, 0.0f,  // 16
         0.125f,  0.125f, -0.125f,  1.0f, 0.0f,  // 17
         0.125f,  0.125f,  0.125f,  1.0f, 1.0f,  // 18
         0.125f, -0.125f,  0.125f,  0.0f, 1.0f,  // 19

         // Left face
         -0.125f, -0.125f, -0.125f,  0.0f, 0.0f,  // 20
         -0.125f,  0.125f, -0.125f,  1.0f, 0.0f,  // 21
         -0.125f,  0.125f,  0.125f,  1.0f, 1.0f,  // 22
         -0.125f, -0.125f,  0.125f,  0.0f, 1.0f   // 23
    };


    unsigned int indices[] = {
        // Front face
        0, 2, 1,   0, 3, 2,

        // Back face
        4, 5, 6,   4, 6, 7,

        // Top face
        8, 9, 10,  8, 10, 11,

        // Bottom face
        12, 14, 13,  12, 15, 14,

        // Right face
        16, 18, 17,  16, 19, 18,

        // Left face
        20, 21, 22,  20, 22, 23
    };



    float topLeftVertices[] = {
  -1.0f,  1.0f,  0.0f, 1.0f, // Top-left
  -0.5f,  1.0f,  1.0f, 1.0f, // Top-right (pomereno dalje udesno)
  -0.5f,  0.8f,  1.0f, 0.0f, // Bottom-right (pomereno dalje udesno i naniže)
  -1.0f,  0.8f,  0.0f, 0.0f  // Bottom-left (pomereno naniže)
    };


    unsigned int topLeftIndices[] = {
        0, 1, 2,  // Prvi trougao
        2, 3, 0   // Drugi trougao
    };

    // VAO, VBO i EBO za kocku
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    //VAO, VBO I EBO za studenta
    unsigned int topLeftVAO, topLeftVBO, topLeftEBO;
    glGenVertexArrays(1, &topLeftVAO);
    glGenBuffers(1, &topLeftVBO);
    glGenBuffers(1, &topLeftEBO);

    glBindVertexArray(topLeftVAO);

    glBindBuffer(GL_ARRAY_BUFFER, topLeftVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(topLeftVertices), topLeftVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, topLeftEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(topLeftIndices), topLeftIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Uniform lokacije
    GLint locM = glGetUniformLocation(shader, "uM");
    GLint locV = glGetUniformLocation(shader, "uV");
    GLint locP = glGetUniformLocation(shader, "uP");

    int N = 10;  // Number of cubes
    float cubeSpacing = glm::pi<float>() / (N - 1);  // Angle between cubes in semi-circle

    studentTexture = loadImageToTexture("res/info.png");
    if (studentTexture == 0)
    {
        std::cout << "Failed to load texture!" << std::endl;
        return 4;
    }

    glBindTexture(GL_TEXTURE_2D, studentTexture);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    unsigned uTexLoc = glGetUniformLocation(studentShader, "uTexture");
    glUseProgram(studentShader);
    glUniform1i(uTexLoc, 0);


    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Face culling setup
    glCullFace(GL_BACK);  // Set which face to cull (back face)
    glFrontFace(GL_CW);  // Set front face winding order (counter-clockwise)

    // Callback za scroll
    glfwSetScrollCallback(window, scroll_callback);
    // Set green background color
    glClearColor(0.0f, 0.5f, 0.0f, 1.0f);  // Dark green color

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);



        // Animaciono vreme
        static float lastTime = 0.0f;  // Static varijabla da zadržimo vrijednost između frame-ova
        if (!paused) {
            lastTime += speed * 0.016f;  // Ažuriramo samo ako nije pauzirano (0.016 je približno 60 FPS)
        }
        float time = lastTime;  // Koristimo zadnju spremljenu vrijednost
        processInput(window);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Fiksna projekcija
        glm::mat4 proj = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);

        // Draw info texture - disable face culling for texture
       // glDisable(GL_CULL_FACE);
        glUseProgram(studentShader);
        glBindVertexArray(topLeftVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, studentTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Switch back to cube shader and setup
        glUseProgram(shader);
        glUniformMatrix4fv(locP, 1, GL_FALSE, glm::value_ptr(proj));

        // Re-enable face culling for cubes if it was enabled
        if (cullingEnabled) {
            glEnable(GL_CULL_FACE);
        }


        // Draw the cubes
        glBindVertexArray(VAO);
        glm::vec3 lastCubePos;
        float lastAngle = 0.0f;

        // Calculate the base angle for the first cube
        float baseAngle = time;

        for (int i = 0; i < N; ++i) {
            // Calculate angle for this cube
            float angle = baseAngle - (i * cubeSpacing);
            float x = cubeRadius * cos(angle);
            float z = cubeRadius * sin(angle);

            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
            glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(model));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

            if (i == N - 1) {
                lastCubePos = glm::vec3(x, 0.0f, z);
            }
        }

        // Calculate camera position relative to the last cube
        float cameraOffsetX = cos(cameraAngle) * cameraDistance;
        float cameraOffsetZ = sin(cameraAngle) * cameraDistance;

        //// Fiksna pozicija kamere
        //glm::vec3 cameraPos = glm::vec3(5.0f, 2.0f, 5.0f);  // Fiksna pozicija


        float radius = 3.0f; // udaljenost kamere od centra (ili poslednjeg vagona)
        glm::vec3 cameraOffset = glm::vec3(
            radius * sin(cameraAngle),
            0.5f,
            radius * cos(cameraAngle)
        );
        glm::vec3 cameraPos = lastCubePos + cameraOffset;


        // Create view matrix that looks at the last cube
        glm::mat4 view = glm::lookAt(
            cameraPos,           // fiksna pozicija kamere
            lastCubePos,         // gleda u poslednju kocku
            glm::vec3(0.0f, 1.0f, 0.0f)  // up vektor
        );
        glUniformMatrix4fv(locV, 1, GL_FALSE, glm::value_ptr(view));


        glDisable(GL_CULL_FACE);
        // Draw the track
        glUseProgram(groundShader);
        glBindVertexArray(trackVAO);
        glm::mat4 trackModel = glm::mat4(1.0f);
        glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(trackModel));
        glUniformMatrix4fv(glGetUniformLocation(groundShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(groundShader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glDrawArrays(GL_TRIANGLE_FAN, 0, trackSegments + 1);

        // Re-enable face culling for cubes if it was enabled
        if (cullingEnabled) {
            glEnable(GL_CULL_FACE);
        }



        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Čišćenje
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &trackVBO);
    glDeleteVertexArrays(1, &trackVAO);
    glDeleteProgram(shader);

    glfwTerminate();
    return 0;
}

unsigned int compileShader(GLenum type, const char* source)
{
    std::string content = "";
    std::ifstream file(source);
    std::stringstream ss;
    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Uspjesno procitao fajl sa putanje \"" << source << "\"!" << std::endl;
    }
    else {
        ss << "";
        std::cout << "Greska pri citanju fajla sa putanje \"" << source << "\"!" << std::endl;
    }
    std::string temp = ss.str();
    const char* sourceCode = temp.c_str();

    int shader = glCreateShader(type);

    int success;
    char infoLog[512];
    glShaderSource(shader, 1, &sourceCode, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        if (type == GL_VERTEX_SHADER)
            printf("VERTEX");
        else if (type == GL_FRAGMENT_SHADER)
            printf("FRAGMENT");
        printf(" sejder ima gresku! Greska: \n");
        printf(infoLog);
    }
    return shader;
}

unsigned int createShader(const char* vsSource, const char* fsSource)
{
    unsigned int program;
    unsigned int vertexShader;
    unsigned int fragmentShader;

    program = glCreateProgram();

    vertexShader = compileShader(GL_VERTEX_SHADER, vsSource);
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource);

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    glValidateProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(program, 512, NULL, infoLog);
        std::cout << "Objedinjeni sejder ima gresku! Greska: \n";
        std::cout << infoLog << std::endl;
    }

    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}
static unsigned loadImageToTexture(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;
    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);
    if (ImageData != NULL)
    {
        //Slike se osnovno ucitavaju naopako pa se moraju ispraviti da budu uspravne
        stbi__vertical_flip(ImageData, TextureWidth, TextureHeight, TextureChannels);

        // Provjerava koji je format boja ucitane slike
        GLint InternalFormat = -1;
        switch (TextureChannels) {
        case 1: InternalFormat = GL_RED; break;
        case 2: InternalFormat = GL_RG; break;
        case 3: InternalFormat = GL_RGB; break;
        case 4: InternalFormat = GL_RGBA; break;
        default: InternalFormat = GL_RGB; break;
        }

        unsigned int Texture;
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, TextureWidth, TextureHeight, 0, InternalFormat, GL_UNSIGNED_BYTE, ImageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        // oslobadjanje memorije zauzete sa stbi_load posto vise nije potrebna
        stbi_image_free(ImageData);
        return Texture;
    }
    else
    {
        std::cout << "Textura nije ucitana! Putanja texture: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return 0;
    }

}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Adjust FOV
    fov -= (float)yoffset;
    if (fov < 10.0f)
        fov = 10.0f;
    if (fov > 65.0f)
        fov = 65.0f;

    //// Adjust camera distance
    //cameraDistance -= (float)yoffset * 0.1f;
    //if (cameraDistance < 0.5f)
    //    cameraDistance = 0.5f;
    //if (cameraDistance > 2.0f)
    //    cameraDistance = 2.0f;
}
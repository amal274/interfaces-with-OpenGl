#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <stb_image.h>
// Screen dimensions
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 768;

// Structure to hold character glyph data
struct Character {
    unsigned int TextureID; 
    glm::ivec2   Size;      
    glm::ivec2   Bearing;   
    unsigned int Advance;   
};

std::map<char, Character> Characters;
unsigned int VAO, VBO;

// Shader sources
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
    out vec2 TexCoords;

    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
        TexCoords = vertex.zw;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in vec2 TexCoords;
    out vec4 color;

    uniform sampler2D text;
    uniform vec3 textColor;

    void main()
    {    
        vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
        color = vec4(textColor, 1.0) * sampled;
    }
)";
const char* textureVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec4 aPosTex; // xy = position, zw = texcoords
    
    out vec2 TexCoords;
    uniform mat4 projection;
    
    void main()
    {
        gl_Position = projection * vec4(aPosTex.xy, 0.0, 1.0);
        TexCoords = aPosTex.zw;
    }
)";

const char* textureFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    in vec2 TexCoords;
    uniform sampler2D textureDiffuse;
    
    void main()
    {
        FragColor = texture(textureDiffuse, TexCoords);
    }
)";

// Simple rectangle drawing shader
const char* rectVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    
    uniform mat4 projection;
    
    void main()
    {
        gl_Position = projection * vec4(aPos, 0.0, 1.0);
    }
)";

const char* rectFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec3 color;
    
    void main()
    {
        FragColor = vec4(color, 1.0);
    }
)";
unsigned int CreateTextureShader() {
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &textureVertexShaderSource, NULL);
    glCompileShader(vertex);

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &textureFragmentShaderSource, NULL);
    glCompileShader(fragment);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}
// Product structure for our mockup
struct Product {
    std::string name;
    std::string price;
    std::string seller;
    unsigned int textureID; 
    float x, y;
};

std::vector<Product> products;

unsigned int shaderProgram, rectShaderProgram;
unsigned int textureShader;
unsigned int LoadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cerr << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
void RenderText(unsigned int shader, std::string text, float x, float y, float scale, glm::vec3 color);
void RenderRect(float x, float y, float width, float height, glm::vec3 color);
void RenderRoundedRect(float x, float y, float width, float height, float radius, glm::vec3 color);
void RenderProductCard(float x, float y, const Product& product);
glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));


int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Marketplace Products Page", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Set viewport
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    // Configure OpenGL
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Compile and setup the text shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Compile rectangle shader
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &rectVertexShaderSource, NULL);
    glCompileShader(vertexShader);

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &rectFragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    rectShaderProgram = glCreateProgram();
    glAttachShader(rectShaderProgram, vertexShader);
    glAttachShader(rectShaderProgram, fragmentShader);
    glLinkProgram(rectShaderProgram);
    textureShader = CreateTextureShader();
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    // FreeType initialization
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, "C:/font/IBM_Plex_Mono/IBMPlexMono-Regular.ttf", 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 characters of ASCII set
    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        // Generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    // Clean up FreeType
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Projection matrix (for converting to screen coordinates)
    
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUseProgram(rectShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(rectShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    float firstRow = 450;
    float secondRow = 180;
    // Create some sample products
    products.push_back({ "Wireless Headphones", "129.99 DT", "AudioTech",LoadTexture("C:/opengl/images/wireless headphones.jpg"), 50, firstRow });
    products.push_back({ "Smart Watch", "199.99 DT", "TechGadgets",LoadTexture("C:/opengl/images/smartwatch.jpg"), 350, firstRow });
    products.push_back({ "Bluetooth Speaker", "79.99 DT", "SoundMaster",LoadTexture("C:/opengl/images/speaker.jpeg"), 650, firstRow });
    products.push_back({ "Laptop Backpack", "49.99 DT", "UrbanGear",LoadTexture("C:/opengl/images/backpack.jpg"), 950, firstRow });
    products.push_back({ "Fitness Tracker", "89.99 DT", "FitLife",LoadTexture("C:/opengl/images/fitness.jpg"), 50, secondRow });
    products.push_back({ "Coffee Maker", "59.99 DT", "BrewPerfect",LoadTexture("C:/opengl/images/coffee.jpg"), 350, secondRow });
    products.push_back({ "Desk Lamp", "34.99 DT", "HomeEssentials",LoadTexture("C:/opengl/images/desk.jpg"), 650, secondRow });
    products.push_back({ "Wireless Mouse", "29.99 DT", "TechAccessories",LoadTexture("C:/opengl/images/mouse.jpg"), 950, secondRow });

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Clear screen
        glClearColor(0.95f, 0.95f, 0.96f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        RenderRect(0, SCR_HEIGHT - 80, SCR_WIDTH, SCR_HEIGHT, glm::vec3(0.95f, 0.95f, 0.96f));
        // Render header
        RenderRect(0, SCR_HEIGHT - 80, SCR_WIDTH, 80, glm::vec3(0.2f, 0.4f, 0.8f));
        RenderText(shaderProgram, "Marketplace", 20, SCR_HEIGHT - 50, 0.8f, glm::vec3(1.0f, 1.0f, 1.0f));

        // Render search bar
        RenderRoundedRect(SCR_WIDTH / 2 - 200, SCR_HEIGHT - 70, 400, 40, 20.0f, glm::vec3(1.0f, 1.0f, 1.0f));
        RenderText(shaderProgram, "Search products...", SCR_WIDTH / 2 - 180, SCR_HEIGHT - 60, 0.4f, glm::vec3(0.5f, 0.5f, 0.5f));

        // Render category tabs
        RenderRect(0, SCR_HEIGHT - 120, SCR_WIDTH, 40, glm::vec3(0.9f, 0.9f, 0.9f));
        RenderText(shaderProgram, "All", 50, SCR_HEIGHT - 110, 0.5f, glm::vec3(0.2f, 0.4f, 0.8f));
        RenderText(shaderProgram, "Electronics", 120, SCR_HEIGHT - 110, 0.5f, glm::vec3(0.4f, 0.4f, 0.4f));
        RenderText(shaderProgram, "Home", 300, SCR_HEIGHT - 110, 0.5f, glm::vec3(0.4f, 0.4f, 0.4f));
        RenderText(shaderProgram, "Fashion", 370, SCR_HEIGHT - 110, 0.5f, glm::vec3(0.4f, 0.4f, 0.4f));
        RenderText(shaderProgram, "Sports", 480, SCR_HEIGHT - 110, 0.5f, glm::vec3(0.4f, 0.4f, 0.4f));

        // Render page title
        RenderText(shaderProgram, "Popular Products", 50, 615, 0.65f, glm::vec3(0.2f, 0.2f, 0.2f));

        // Render all products
        for (const auto& product : products) {
            RenderProductCard(product.x, product.y, product);
        }

        // Render footer
        RenderRect(0, 0, SCR_WIDTH, 60, glm::vec3(0.9f, 0.9f, 0.9f));
        RenderText(shaderProgram, "Home", 50, 20, 0.4f, glm::vec3(0.2f, 0.4f, 0.8f));
        RenderText(shaderProgram, "Search", 150, 20, 0.4f, glm::vec3(0.4f, 0.4f, 0.4f));
        RenderText(shaderProgram, "Cart", 250, 20, 0.4f, glm::vec3(0.4f, 0.4f, 0.4f));
        RenderText(shaderProgram, "Profile", 350, 20, 0.4f, glm::vec3(0.4f, 0.4f, 0.4f));

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(rectShaderProgram);

    glfwTerminate();
    return 0;
}
void RenderTexture(unsigned int texture, float x, float y, float width, float height) {
    glUseProgram(rectShaderProgram);
    glUniform3f(glGetUniformLocation(rectShaderProgram, "color"), 1.0f, 1.0f, 1.0f);

    float vertices[] = {
        x, y, 0.0f, 0.0f,
        x, y + height, 0.0f, 1.0f,
        x + width, y + height, 1.0f, 1.0f,
        x + width, y, 1.0f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    unsigned int texVAO, texVBO, texEBO;
    glGenVertexArrays(1, &texVAO);
    glGenBuffers(1, &texVBO);
    glGenBuffers(1, &texEBO);

    glBindVertexArray(texVAO);

    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, texEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDeleteVertexArrays(1, &texVAO);
    glDeleteBuffers(1, &texVBO);
    glDeleteBuffers(1, &texEBO);
}
void RenderTexture(unsigned int textureShader, unsigned int texture,
    float x, float y, float width, float height) {
    glUseProgram(textureShader);
    glUniformMatrix4fv(glGetUniformLocation(textureShader, "projection"),
        1, GL_FALSE, glm::value_ptr(projection));

    float vertices[] = {
        // positions     // texture coords (flipped vertically)
        x, y,           0.0f, 1.0f,  // Changed from 0.0f, 0.0f
        x, y + height,  0.0f, 0.0f,  // Changed from 0.0f, 1.0f
        x + width, y + height, 1.0f, 0.0f,  // Changed from 1.0f, 1.0f
        x + width, y,    1.0f, 1.0f   // Changed from 1.0f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void RenderText(unsigned int shader, std::string text, float x, float y, float scale, glm::vec3 color) {
    // Activate corresponding render state
    glUseProgram(shader);
    glUniform3f(glGetUniformLocation(shader, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Now advance cursors for next glyph
        x += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderRect(float x, float y, float width, float height, glm::vec3 color) {
    glUseProgram(rectShaderProgram);
    glUniform3f(glGetUniformLocation(rectShaderProgram, "color"), color.x, color.y, color.z);

    float vertices[] = {
        x, y,
        x, y + height,
        x + width, y + height,
        x + width, y
    };

    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    unsigned int rectVAO, rectVBO, rectEBO;
    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);
    glGenBuffers(1, &rectEBO);

    glBindVertexArray(rectVAO);

    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rectEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDeleteVertexArrays(1, &rectVAO);
    glDeleteBuffers(1, &rectVBO);
    glDeleteBuffers(1, &rectEBO);
}

void RenderRoundedRect(float x, float y, float width, float height, float radius, glm::vec3 color) {
    // For simplicity, we'll just render a regular rectangle in this example
    // A proper rounded rectangle would require more complex geometry or a shader
    RenderRect(x, y, width, height, color);
}
void RenderProductCard(float x, float y, const Product& product) {
    RenderTexture(textureShader, product.textureID,
        product.x, product.y, 150, 150);

    // Product name
    RenderText(shaderProgram, product.name, x, y - 25, 0.4f, glm::vec3(0.2f, 0.2f, 0.2f));

    // Product price
    RenderText(shaderProgram, product.price, x, y - 48, 0.4f, glm::vec3(0.2f, 0.4f, 0.8f));

    // Seller info
    RenderText(shaderProgram, "Sold by " + product.seller, x, y - 63, 0.3f, glm::vec3(0.5f, 0.5f, 0.5f));

    // "Add to cart" button
    RenderRoundedRect(x+25, y - 105, 90, 30, 15.0f, glm::vec3(0.2f, 0.4f, 0.8f));
    RenderText(shaderProgram, "Add to Cart", x+30, y - 95, 0.25f, glm::vec3(1.0f, 1.0f, 1.0f));
}
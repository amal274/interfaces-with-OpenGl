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
	unsigned int TextureID; // ID handle of the glyph texture
	glm::ivec2   Size;      // Size of glyph
	glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
	unsigned int Advance;   // Horizontal offset to advance to next glyph
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
const char* roundedRectVertexShader = R"(
    #version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}

)";

const char* roundedRectFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec2 uSize;
    uniform vec2 uPosition;
    uniform float uRadius;
    uniform vec3 uColor;
    
    float roundedBoxSDF(vec2 centerPos, vec2 size, float radius) {
        vec2 q = abs(centerPos) - size + radius;
        return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
    }
    
    void main()
    {
        vec2 pixelCoord = gl_FragCoord.xy;
        vec2 center = uPosition + uSize/2.0;
        vec2 halfSize = uSize/2.0;
        
        float distance = roundedBoxSDF(pixelCoord - center, halfSize, uRadius);
        
        if (distance > 0.0) discard;
        
        FragColor = vec4(uColor, 1.0);
    }
)";
// Vertex Shader
const char* circleImageVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    out vec2 TexCoord;
    out vec2 FragPos;
    
    uniform mat4 projection;
    uniform mat4 model;
    
    void main()
    {
        vec4 worldPos = model * vec4(aPos, 0.0, 1.0);
        FragPos = worldPos.xy;
        gl_Position = projection * worldPos;
        TexCoord = aTexCoord;
    }
)";

const char* circleImageFragmentShader = R"(
    #version 330 core
    in vec2 TexCoord;
    in vec2 FragPos;
    out vec4 FragColor;
    
    uniform sampler2D imageTexture;
    uniform float radius;
    
    void main()
    {
        // Calculate distance from center
        vec2 center = vec2(0.0);
        float dist = length(FragPos - center);
        
        // Discard pixels outside circle with anti-aliasing
        float edgeSoftness = 2.0;
        float alpha = smoothstep(radius + edgeSoftness, radius - edgeSoftness, dist);
        
        // Sample texture
        vec4 texColor = texture(imageTexture, TexCoord);
        
        // Final color with validation
        FragColor = vec4(texColor.rgb, texColor.a * alpha);
        
        // Validation checks (corrected)
        if (isnan(FragColor.r)) {  // Added missing parenthesis
            FragColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta for NaN values
        }
        if (FragColor.a < 0.01) discard;
    }
)";
unsigned int CreateShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource);
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

struct Message {
	std::string name;
	std::string message;
	std::string time;
	unsigned int textureID; 
	int order;
};

std::vector<Product> products;
std::vector<Message> messages;

unsigned int shaderProgram, rectShaderProgram;
unsigned int textureShader;
unsigned int LoadTexture(const char* path) {
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	// Corrected function name here:
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
unsigned int LoadTextureCircular(const std::string& path) {
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);

	if (data) {
		GLenum format;
		if (nrComponents == 1) format = GL_RED;
		else if (nrComponents == 3) format = GL_RGB;
		else if (nrComponents == 4) format = GL_RGBA;

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
		std::cerr << "Failed to load texture at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}
void RenderCircularImage(unsigned int texture, float x, float y, float diameter) {
	static unsigned int shaderProgram = 0;
	static unsigned int VAO = 0;

	// 1. Initialize shader and VAO
	if (shaderProgram == 0) {
		shaderProgram = CreateShaderProgram(circleImageVertexShader, circleImageFragmentShader);
		if (shaderProgram == 0) {
			std::cerr << "Failed to create shader program!" << std::endl;
			return;
		}

		float vertices[] = {
			-0.5f, -0.5f, 0.0f, 0.0f,
			 0.5f, -0.5f, 1.0f, 0.0f,
			 0.5f,  0.5f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 1.0f
		};
		unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

		glGenVertexArrays(1, &VAO);
		unsigned int VBO, EBO;
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);
	}

	// 2. Set transformations
	glm::mat4 projection = glm::ortho(0.0f, (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(x, y, 0.0f));
	model = glm::scale(model, glm::vec3(diameter, diameter, 1.0f));

	// 3. Set shader uniforms
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
	glUniform1f(glGetUniformLocation(shaderProgram, "radius"), 0.5f);

	// 4. Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(glGetUniformLocation(shaderProgram, "imageTexture"), 0);
	// 5. Draw with blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	// 6. Error checking
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error: " << err << std::endl;
	}
}
void RenderText(unsigned int shader, std::string text, float x, float y, float scale, glm::vec3 color);
void RenderRect(float x, float y, float width, float height, glm::vec3 color);
void RenderRoundedRect(float x, float y, float width, float height, float radius, glm::vec3 color);
void RenderProductCard(float x, float y, const Product& product);
void RenderMessageCard(Message message);
glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
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
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Chat messages", NULL, NULL);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	gladLoadGL();
	// Initialize GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Set viewport
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);


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
	messages.push_back({ "Amel", "Bonsoir", "19:03",LoadTexture("C:/opengl/images/face1.png"), 6 });
	messages.push_back({ "Ahmed", "Comment Vas tu?", "17:53",LoadTexture("C:/opengl/images/face2.png"), 5 });
	messages.push_back({ "Nour", "Super !", "16:22",LoadTexture("C:/opengl/images/face3.png"), 4 });
	messages.push_back({ "Mourad", "Exactement ce mood que je ressens...", "13:30",LoadTexture("C:/opengl/images/face4.png"), 3 });
	messages.push_back({ "Kais", "C'est ou ca?", "11:09",LoadTexture("C:/opengl/images/face5.png"), 2 });
	messages.push_back({ "Lina", "Bonjour", "07:42",LoadTexture("C:/opengl/images/face6.png"), 1 });
	unsigned int image = LoadTexture("C:/opengl/images/face3.png");
	// Main loop
	while (!glfwWindowShouldClose(window)) {
		// Clear screen
		glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		RenderRect(0, 0, SCR_WIDTH / 2.5, SCR_HEIGHT, glm::vec3(0.09f, 0.13f, 0.17f));
		
		RenderRoundedRect(10.0f, SCR_HEIGHT - 70, (SCR_WIDTH / 2.5) - 20, 50.0f, 15.0f, glm::vec3(0.14f, 0.18f, 0.24f));
		RenderText(shaderProgram, "Recherche...", 20, SCR_HEIGHT - 55, 0.45f, glm::vec3(0.43f, 0.47f, 0.51f));
		
		// Render all products
		for (const auto& message : messages) {
		    RenderMessageCard(message);
		}
		//header
		RenderRect(SCR_WIDTH / 2.5, SCR_HEIGHT - 90, SCR_WIDTH - (SCR_WIDTH / 2.5), 90, glm::vec3(0.09f, 0.13f, 0.17f));
		RenderTexture(textureShader, image, SCR_WIDTH / 2.5 + 20, SCR_HEIGHT - 80, 60, 60);
		RenderText(shaderProgram, "Nour", SCR_WIDTH / 2.5 + 100, SCR_HEIGHT - 60, 0.6, glm::vec3(1, 1, 1));
		


		//send message
		RenderRect(SCR_WIDTH / 2.5, 0, SCR_WIDTH - (SCR_WIDTH / 2.5), 90, glm::vec3(0.09f, 0.13f, 0.17f));
		RenderRoundedRect(SCR_WIDTH / 2.5 + 10, 20, SCR_WIDTH - (SCR_WIDTH / 2.5) - 120, 50.0f, 15.0f, glm::vec3(0.14f, 0.18f, 0.24f));
		RenderRoundedRect(SCR_WIDTH - 100, 20, 90, 50.0f, 15.0f, glm::vec3(0.169, 0.322, 0.471));
		RenderText(shaderProgram, "Envoyer", SCR_WIDTH - 95, 37, 0.4, glm::vec3(1, 1, 1));
		RenderText(shaderProgram, "Tapez un message...", SCR_WIDTH / 2.5 + 20, 37, 0.4, glm::vec3(0.43f, 0.47f, 0.51f));
		//messages
		RenderRoundedRect(SCR_WIDTH / 2.5 + 20, SCR_HEIGHT - 150, 110, 50.0f, 15.0f, glm::vec3(0.14f, 0.18f, 0.24f));
		RenderText(shaderProgram, "Bonjour", SCR_WIDTH / 2.5 + 30, SCR_HEIGHT - 133, 0.4, glm::vec3(1, 1, 1));

		RenderRoundedRect(SCR_WIDTH - 120, SCR_HEIGHT - 220, 110, 50.0f, 15.0f, glm::vec3(0.169, 0.322, 0.471));
		RenderText(shaderProgram, "Bonjour", SCR_WIDTH - 110, SCR_HEIGHT - 203, 0.4, glm::vec3(1, 1, 1));

		RenderRoundedRect(SCR_WIDTH / 2.5 + 20, SCR_HEIGHT - 290, 110, 50.0f, 15.0f, glm::vec3(0.14f, 0.18f, 0.24f));
		RenderText(shaderProgram, "Ca va?", SCR_WIDTH / 2.5 + 30, SCR_HEIGHT - 273, 0.4, glm::vec3(1, 1, 1));

		RenderRoundedRect(SCR_WIDTH - 180, SCR_HEIGHT - 360, 170, 50.0f, 15.0f, glm::vec3(0.169, 0.322, 0.471));
		RenderText(shaderProgram, "Ca va et toi?", SCR_WIDTH - 170, SCR_HEIGHT - 343, 0.4, glm::vec3(1, 1, 1));

		RenderRoundedRect(SCR_WIDTH / 2.5 + 20, SCR_HEIGHT - 430, 110, 50.0f, 15.0f, glm::vec3(0.14f, 0.18f, 0.24f));
		RenderText(shaderProgram, "Super !", SCR_WIDTH / 2.5 + 30, SCR_HEIGHT - 413, 0.4, glm::vec3(1, 1, 1));
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

void RenderMessageCard(Message message) {
	if (message.order == 4) {
		RenderRect(0, 585 - ((6 - message.order) * 115), SCR_WIDTH / 2.5, 100, glm::vec3(0.169, 0.322, 0.471));
	}
	RenderTexture(textureShader, message.textureID,
		10, 590 - ((6 - message.order) * 115), 90, 90);
	RenderText(shaderProgram, message.time, 435, 650 - ((6 - message.order) * 115), 0.25, glm::vec3(0.43f, 0.47f, 0.51f));
	RenderText(shaderProgram, message.name, 115, 650 - ((6 - message.order) * 115), 0.4, glm::vec3(1, 1, 1));
	RenderText(shaderProgram, message.message, 115, 615 - ((6 - message.order) * 115), 0.35, glm::vec3(0.43f, 0.47f, 0.51f));
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

unsigned int roundedRectShader = 0;
unsigned int roundedRectVAO = 0;
unsigned int roundedRectVBO = 0;
unsigned int CreateShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource) {
	// Create and compile vertex shader
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	// Check vertex shader compilation
	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
	}

	// Create and compile fragment shader
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	// Check fragment shader compilation
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
	}

	// Create shader program and link shaders
	unsigned int shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	// Check linking errors
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
	}

	// Clean up shaders
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	// Enhanced error checking
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cerr << "VERTEX SHADER COMPILATION FAILED:\n" << infoLog << std::endl;
		return 0;
	}

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cerr << "FRAGMENT SHADER COMPILATION FAILED:\n" << infoLog << std::endl;
		return 0;
	}

	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cerr << "SHADER PROGRAM LINKING FAILED:\n" << infoLog << std::endl;
		return 0;
	}
	return shaderProgram;
}
void InitializeRoundedRectRenderer() {
	// Compile shaders
	roundedRectShader = CreateShaderProgram(roundedRectVertexShader, roundedRectFragmentShader);

	// Fullscreen quad VAO
	float vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f
	};

	glGenVertexArrays(1, &roundedRectVAO);
	glGenBuffers(1, &roundedRectVBO);

	glBindVertexArray(roundedRectVAO);
	glBindBuffer(GL_ARRAY_BUFFER, roundedRectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void RenderRoundedRect(float x, float y, float width, float height, float radius, glm::vec3 color) {
	if (roundedRectShader == 0) {
		InitializeRoundedRectRenderer();
	}

	glUseProgram(roundedRectShader);

	// Set uniforms
	glm::mat4 projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
	glUniformMatrix4fv(glGetUniformLocation(roundedRectShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	glUniform2f(glGetUniformLocation(roundedRectShader, "uPosition"), x, y);
	glUniform2f(glGetUniformLocation(roundedRectShader, "uSize"), width, height);
	glUniform1f(glGetUniformLocation(roundedRectShader, "uRadius"), radius);
	glUniform3f(glGetUniformLocation(roundedRectShader, "uColor"), color.r, color.g, color.b);
	glUniform2f(glGetUniformLocation(roundedRectShader, "uScreenSize"), SCR_WIDTH, SCR_HEIGHT);

	// Render fullscreen quad (shader will handle the actual shape)
	glBindVertexArray(roundedRectVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
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
	RenderRoundedRect(x + 25, y - 105, 90, 30, 15.0f, glm::vec3(0.2f, 0.4f, 0.8f));
	RenderText(shaderProgram, "Add to Cart", x + 30, y - 95, 0.25f, glm::vec3(1.0f, 1.0f, 1.0f));
}
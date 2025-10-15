🖥️ OpenGL Graphical Interfaces Project
📖 Overview

This project showcases two custom 2D graphical interfaces built entirely in OpenGL.
It demonstrates how to create modern, UI-like layouts (such as marketplace and chat screens) using shaders, shapes, textures, and text rendering — without relying on a traditional GUI framework.

The project includes:

1. 🛍️ Marketplace Interface — a clean product display with images, prices, and navigation.

2. 💬 Chat Interface — a realistic messaging layout with circular profile images, message bubbles, and text rendering.

Each interface is designed using the same OpenGL rendering engine and can be switched or extended to create a full multi-screen application.

🧩 Features

1. 🛍️ Marketplace Interface

*  Product cards with images, titles, prices, and seller info

*  Header with search bar and category navigation

*  Footer bar for navigation icons (Home, Cart, Profile, etc.)

*  Rounded rectangles and image textures for modern design

*  Text rendering with FreeType for titles and labels

2. 💬 Chat Interface

*  Circular user avatars using texture masking

*  Speech bubble rendering with rounded rectangles

*  Timestamp and username text with multiple font sizes

*  Layout supporting both incoming and outgoing messages

*  Uses FreeType for all text and stb_image for images

🧱 Technologies Used

* OpenGL 3.3 Core :	Rendering engine
* GLFW :	Window and input management
* GLAD :	OpenGL function loader
* GLM :	Matrix and vector math
* stb_image.h :	Image loading
* FreeType :	Font rendering


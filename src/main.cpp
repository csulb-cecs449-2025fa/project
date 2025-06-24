/**
* Starter code for CECS 449 final project.
* Major modules:
*	Mesh: an OpenGL VAO associated with one or more textures. Transfers vertices from RAM to VRAM. (INCOMPLETE)
*   Object3D: pairs a Mesh with world-space parameters (position, orientation, scale), and methods to manipulate
*			  the object in world space.
*			  Object3D is hierarchical; a "hierarchical mesh" is actually a tree of Object3Ds, which render
*			  recursively. (INCOMPLETE)
*   AssimpImport: loads an assimp model file into an hierarchical Object3D. (INCOMPLETE)
*   Animator: a list of animations to apply over given time intervals.
*   Main: initializes a Scene, advances Animators, and renders objects in the scene.
*/
#include <glad/glad.h>
#include <iostream>
#include <memory>
#include <filesystem>
#include <numbers>

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>
#include <SFML/Graphics.hpp>

#include "AssimpImport.h"
#include "Mesh.h"
#include "Object3D.h"
#include "Animator.h"
#include "ShaderProgram.h"

#define M_PI std::numbers::pi_v<float>

// We use a structure to track all the elements of a scene, including a list of objects,
// a list of animators, and a shader program to use to render those objects.
struct Scene {
	ShaderProgram program{};
	std::vector<Object3D> objects{};
	std::vector<Animator> animators{};
};

/**
 * @brief Constructs a shader program that applies the Phong reflection model.
 */
ShaderProgram phongLightingShader() {
	ShaderProgram shader{};
	try {
		// These shaders are INCOMPLETE.
		shader.load("shaders/light_perspective.vert", "shaders/lighting.frag");
	}
	catch (std::runtime_error& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		exit(1);
	}
	return shader;
}

/**
 * @brief Constructs a shader program that performs texture mapping with no lighting.
 */
ShaderProgram texturingShader() {
	ShaderProgram shader{};
	try {
		shader.load("shaders/texture_perspective.vert", "shaders/texturing.frag");
	}
	catch (std::runtime_error& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		exit(1);
	}
	return shader;
}

/**
 * @brief Loads an image from the given path into an OpenGL texture.
 */
Texture loadTexture(const std::filesystem::path& path, const std::string& samplerName = "baseTexture") {
	StbImage i{};
	i.loadFromFile(path.string());
	return Texture::loadImage(i, samplerName);
}

/*****************************************************************************************
*  DEMONSTRATION SCENES
*****************************************************************************************/
Scene bunny() {
	Scene scene{ texturingShader() };

	// We assume that (0,0) in texture space is the upper left corner, but some artists use (0,0) in the lower
	// left corner. In that case, we have to flip the V-coordinate of each UV texture location. The last parameter
	// to assimpLoad controls this. If you load a model and it looks very strange, try changing the last parameter.
	auto bunny{ assimpLoad("models/bunny_textured.obj", true) };
	bunny.grow(glm::vec3{ 9, 9, 9 });
	bunny.move(glm::vec3{ 0.2, -1, 0 });

	// Move all objects into the scene's objects list.
	scene.objects.push_back(std::move(bunny));
	// Now the "bunny" variable is empty; if we want to refer to the bunny object, we need to reference 
	// scene.objects[0]

	Animator spinBunny{};
	// Spin the bunny 360 degrees over 10 seconds.
	spinBunny.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0f, glm::vec3{ 0, 1, 0 }));

	// Move all animators into the scene's animators list.
	scene.animators.push_back(std::move(spinBunny));

	return scene;
}


/**
 * @brief Demonstrates loading a square, oriented as the "floor", with a manually-specified texture
 * that does not come from Assimp.
 */
Scene marbleSquare() {
	Scene scene{ texturingShader() };

	std::vector<Texture> textures{
		loadTexture("models/White_marble_03/Textures_2K/white_marble_03_2k_baseColor.tga", "baseTexture"),
	};
	auto mesh{ Mesh::square(textures) };
	Object3D floor{ std::vector<Mesh>{mesh} };
	floor.grow(glm::vec3{ 5, 5, 5 });
	floor.move(glm::vec3{ 0, -1.5, 0 });
	floor.rotate(glm::vec3{ -M_PI / 2, 0, 0 });

	scene.objects.push_back(std::move(floor));
	return scene;
}

/**
 * @brief Loads a cube with a cube map texture.
 */
Scene cube() {
	Scene scene{ texturingShader() };

	auto cube{ assimpLoad("models/cube.obj", true) };

	scene.objects.push_back(std::move(cube));

	Animator spinCube{};
	spinCube.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0f, glm::vec3{ 0, 2 * M_PI, 0 }));
	// Then spin around the x axis.
	spinCube.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0f, glm::vec3{ 2 * M_PI, 0, 0 }));

	scene.animators.push_back(std::move(spinCube));

	return scene;
}

/**
 * @brief Constructs a scene of a tiger sitting in a boat, where the tiger is the child object
 * of the boat.
 * @return
 */
Scene lifeOfPi() {
	// This scene is more complicated; it has child objects, as well as animators.
	Scene scene{ texturingShader() };

	auto boat{ assimpLoad("models/boat/boat.fbx", true) };
	boat.move(glm::vec3{ 0, -0.7, 0 });
	boat.grow(glm::vec3{ 0.01, 0.01, 0.01 });
	auto tiger{ assimpLoad("models/tiger/scene.gltf", true) };
	tiger.move(glm::vec3{ 0, -5, 10 });
	// Move the tiger to be a child of the boat.
	boat.addChild(std::move(tiger));

	// Move the boat into the scene list.
	scene.objects.push_back(std::move(boat));

	// We want these animations to referenced the *moved* objects, which are no longer
	// in the variables named "tiger" and "boat". "boat" is now in the "objects" list at
	// index 0, and "tiger" is the index-1 child of the boat.
	Animator animBoat{};
	animBoat.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0], 10.0f, glm::vec3{ 0, 2 * M_PI, 0 }));
	Animator animTiger{};
	animTiger.addAnimation(std::make_unique<RotationAnimation>(scene.objects[0].getChild(1), 10.0f, glm::vec3{ 0, 0, 2 * M_PI }));

	// The Animators will be destroyed when leaving this function, so we move them into
	// a list to be returned.
	scene.animators.push_back(std::move(animBoat));
	scene.animators.push_back(std::move(animTiger));

	// Transfer ownership of the objects and animators back to the main.
	return scene;
}



int main() {

	std::cout << std::filesystem::current_path() << std::endl;

	// Initialize the window and OpenGL.
	sf::ContextSettings settings;
	settings.depthBits = 24; // Request a 24 bits depth buffer
	settings.stencilBits = 8;  // Request a 8 bits stencil buffer
	settings.majorVersion = 3; // You might have to change these on Mac.
	settings.minorVersion = 3;

	sf::Window window{
		sf::VideoMode::getFullscreenModes().at(0), "Modern OpenGL",
		sf::Style::Resize | sf::Style::Close,
		sf::State::Windowed, settings
	};

	gladLoadGL();
	glEnable(GL_DEPTH_TEST);

	// Inintialize scene objects.
	auto myScene{ bunny() };
	// You can directly access specific objects in the scene using references.
	auto& firstObject{ myScene.objects[0] };

	// Activate the shader program.
	myScene.program.activate();

	// Start the animators.
	for (auto& anim : myScene.animators) {
		anim.start();
	}

	// Ready, set, go!
	bool running{ true };
	sf::Clock c;

	auto last{ c.getElapsedTime() };
	while (window.isOpen()) {
		// Check for events.
		while (const std::optional event{ window.pollEvent() }) {
			if (event->is<sf::Event::Closed>()) {
				window.close();
			}
		}
		auto now{ c.getElapsedTime() };
		auto diff{ now - last };
		last = now;

#ifdef LOG_FPS
		// FPS calculation.
		std::cout << 1 / diff.asSeconds() << " FPS " << std::endl;
#endif

		glm::vec3 cameraPos{ glm::vec3{ 0, 0, 5 } };
		glm::mat4 camera{ glm::lookAt(cameraPos, glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 1, 0 }) };
		glm::mat4 perspective{ glm::perspective(glm::radians(45.0f), static_cast<float>(window.getSize().x) / window.getSize().y, 0.1f, 100.0f) };
		myScene.program.setUniform("view", camera);
		myScene.program.setUniform("projection", perspective);
		myScene.program.setUniform("cameraPos", cameraPos);

		// Update the scene.
		for (auto& anim : myScene.animators) {
			anim.tick(diff.asSeconds());
		}

		// Clear the OpenGL "context".
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Render the scene objects.
		for (auto& o : myScene.objects) {
			o.render(myScene.program);
		}
		window.display();
	}

	return 0;
}



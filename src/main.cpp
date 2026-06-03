#include <vk_engine.h>
#undef main // fixes linker error with sdl

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();

	engine.cleanup();	

	return 0;
}

/*TODO: 
* move camera
* PBR shading + more textures
* try to speed up startup time (maybe not...)
* arrows to switch mat
* antialiasing
* gamma correction
*/
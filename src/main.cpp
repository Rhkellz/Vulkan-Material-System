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
* try to speed up startup time (maybe not...)
* arrows to switch mat
* antialiasing
* gamma correction
* custom samplers?
* POM
* GPU accelerated texture loading
* Vector Displacement Mapping?
* tiling scale
*/
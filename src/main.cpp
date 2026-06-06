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
* antialiasing
* gamma correction
* custom samplers?
* GPU accelerated texture loading
* tiling scale
* multithreading?
*/

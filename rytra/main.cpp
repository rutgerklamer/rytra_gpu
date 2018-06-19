// OpenCL based simple sphere path tracer by Sam Lapere, 2016
// based on smallpt by Kevin Beason 
// http://raytracey.blogspot.com 

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <CL/cl.hpp>
#include <SDL2/SDL.h>

using namespace std;
using namespace cl;

const int image_width = 1200;
const int image_height = 600;

cl_float4* cpu_output;
CommandQueue queue;
Device device;
Kernel kernel;
Context context;
Program program;
Buffer cl_output;
Buffer cl_spheres;

SDL_Renderer* renderer;
SDL_Event event;
// dummy variables are required for memory alignment
// float3 is considered as float4 by OpenCL
struct Sphere
{
	cl_float radius;
	cl_float dummy1;   
	cl_float dummy2;
	cl_float dummy3;
	cl_float3 position;
	cl_float3 color;
	cl_float3 emission;
};

void pickPlatform(Platform& platform, const vector<Platform>& platforms){

	if (platforms.size() == 1) platform = platforms[0];
	else{
		int input = 0;
		cout << "\nChoose an OpenCL platform: ";
		cin >> input;

		// handle incorrect user input
		while (input < 1 || input > platforms.size()){
			cin.clear(); //clear errors/bad flags on cin
			cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			cout << "No such option. Choose an OpenCL platform: ";
			cin >> input;
		}
		platform = platforms[input - 1];
	}
}

void pickDevice(Device& device, const vector<Device>& devices){

	if (devices.size() == 1) device = devices[0];
	else{
		int input = 0;
		cout << "\nChoose an OpenCL device: ";
		cin >> input;

		// handle incorrect user input
		while (input < 1 || input > devices.size()){
			cin.clear(); //clear errors/bad flags on cin
			cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			cout << "No such option. Choose an OpenCL device: ";
			cin >> input;
		}
		device = devices[input - 1];
	}
}

void initSDL2()
{
	SDL_Window* window;
	SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(image_width, image_height, 0, &window, &renderer);
}

void initOpenCL()
{
	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	vector<Platform> platforms;
	Platform::get(&platforms);
	cout << "Available OpenCL platforms : " << endl << endl;
	for (int i = 0; i < platforms.size(); i++)
		cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

	// Pick one platform
	Platform platform;
	pickPlatform(platform, platforms);
	cout << "\nUsing OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

	// Get available OpenCL devices on platform
	vector<Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	cout << "Available OpenCL devices on this platform: " << endl << endl;
	for (int i = 0; i < devices.size(); i++){
		cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << endl;
		cout << "\t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
		cout << "\t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl << endl;
	}

	// Pick one device
	pickDevice(device, devices);
	cout << "\nUsing OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << endl;
	cout << "\t\t\tMax compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
	cout << "\t\t\tMax work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl;

	// Create an OpenCL context and command queue on that device.
	context = Context(device);
	queue = CommandQueue(context, device);

	// Convert the OpenCL source code to a string
	string source;
	ifstream file("rytra.cl");
	if (!file){
		cout << "\nNo OpenCL file found!" << endl << "Exiting..." << endl;
		system("PAUSE");
		exit(1);
	}
	while (!file.eof()){
		char line[256];
		file.getline(line, 255);
		source += line;
	}

	const char* kernel_source = source.c_str();

	// Create an OpenCL program by performing runtime source compilation for the chosen device
	program = Program(context, kernel_source);
	cl_int result = program.build({ device });
	if (result) cout << "Error during compilation OpenCL code!!!\n (" << result << ")" << endl;

	// Create a kernel (entry point in the OpenCL source program)
	kernel = Kernel(program, "render_kernel");
}

void cleanUp(){
	delete cpu_output;
}

inline float clamp(float x){ return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
inline int toInt(float x){ return int(clamp(x) * 255.99); }

void saveImage(){
	// write image to PPM file, a very simple image file format
	// PPM files can be opened with IrfanView (download at www.irfanview.com) or GIMP
	FILE *f = fopen("opencl_raytracer.ppm", "w");
	fprintf(f, "P3\n%d %d\n%d\n", image_width, image_height, 255);

	// loop over all pixels, write RGB values
	for (int i = 0; i < image_width * image_height; i++)
		fprintf(f, "%d %d %d ",
		toInt(cpu_output[i].s[0]),
		toInt(cpu_output[i].s[1]),
		toInt(cpu_output[i].s[2]));
}

#define float3(x, y, z) {{x, y, z}}  // macro to replace ugly initializer braces

void initScene(Sphere* cpu_spheres){

	// left wall
	cpu_spheres[0].radius	= 200.0f;
	cpu_spheres[0].position = float3(-200.6f, 0.0f, 0.0f);
	cpu_spheres[0].color    = float3(0.75f, 0.25f, 0.25f);
	cpu_spheres[0].emission = float3(0.0f, 0.0f, 0.0f);

	// right wall
	cpu_spheres[1].radius	= 200.0f;
	cpu_spheres[1].position = float3(200.6f, 0.0f, 0.0f);
	cpu_spheres[1].color    = float3(0.25f, 0.25f, 0.75f);
	cpu_spheres[1].emission = float3(0.0f, 0.0f, 0.0f);

	// floor
	cpu_spheres[2].radius	= 200.0f;
	cpu_spheres[2].position = float3(0.0f, -200.4f, 0.0f);
	cpu_spheres[2].color	= float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[2].emission = float3(0.0f, 0.0f, 0.0f);

	// ceiling
	cpu_spheres[3].radius	= 200.0f;
	cpu_spheres[3].position = float3(0.0f, 200.4f, 0.0f);
	cpu_spheres[3].color	= float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[3].emission = float3(0.0f, 0.0f, 0.0f);

	// back wall
	cpu_spheres[4].radius   = 200.0f;
	cpu_spheres[4].position = float3(0.0f, 0.0f, -200.4f);
	cpu_spheres[4].color    = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[4].emission = float3(0.0f, 0.0f, 0.0f);

	// front wall 
	cpu_spheres[5].radius   = 200.0f;
	cpu_spheres[5].position = float3(0.0f, 0.0f, 202.0f);
	cpu_spheres[5].color    = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[5].emission = float3(0.0f, 0.0f, 0.0f);

	// left sphere
	cpu_spheres[6].radius   = 0.16f;
	cpu_spheres[6].position = float3(-0.25f, -0.24f, -0.1f);
	cpu_spheres[6].color    = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[6].emission = float3(0.0f, 0.0f, 0.0f);

	// right sphere
	cpu_spheres[7].radius   = 0.16f;
	cpu_spheres[7].position = float3(0.25f, -0.24f, 0.1f);
	cpu_spheres[7].color    = float3(0.9f, 0.8f, 0.7f);
	cpu_spheres[7].emission = float3(0.0f, 0.0f, 0.0f);

	// lightsource
	cpu_spheres[8].radius   = 1.0f;
	cpu_spheres[8].position = float3(0.0f, 1.36f, 0.0f);
	cpu_spheres[8].color    = float3(0.0f, 0.0f, 0.0f);
	cpu_spheres[8].emission = float3(9.0f, 8.0f, 6.0f);
	
}

void draw_pixel(SDL_Renderer* r, float cr, float cg, float cb, float px, float py)
{
	SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawPoint(r, px, py); 
}

int main() {
	initOpenCL();
	initSDL2();
	// allocate memory on CPU to hold the rendered image
	cpu_output = new cl_float3[image_width * image_height];

	// Create buffers on the OpenCL device for the image and the scene
	cl_output = Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));

	// specify OpenCL kernel arguments
	kernel.setArg(0, cl_output);
	kernel.setArg(1, image_width);
	kernel.setArg(2, image_height);

	// every pixel in the image has its own thread or "work item",
	// so the total amount of work items equals the number of pixels
	std::size_t global_work_size = image_width * image_height;
	std::size_t local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device);
	
	cout << "Kernel work group size: " << local_work_size << endl;

	// Ensure the global work size is a multiple of local work size
	 if (global_work_size % local_work_size != 0)
		global_work_size = (global_work_size / local_work_size + 1) * local_work_size;

	 Uint32 pixels[image_width*image_height];
	 long double delta_time;
	 float timer = 0.0f;
	 int fps = 0;
	 bool quit = false;
	 float y = 0.5f;
	 float x = 0.0f;
	 while (!quit) {
	kernel.setArg(3, int(rand()%2028));
	kernel.setArg(4, y);
	kernel.setArg(5, x);

		 fps++;
		 timer+=delta_time;

		 if (timer > 1.0f) {
			 timer = 0;
			 std::cout << " FPS ( " << fps << " ) DT ( " << delta_time << " ) " << std::endl;
			 fps = 0;
		 }
		 clock_t begin =  clock ();
		 

		// launch the kernel
		queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size);


		// read and copy OpenCL output to CPU
		queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_float3), cpu_output);
		for (int i = 0; i < image_width * image_height; i++) {
					Uint32 ir = Uint32(toInt(cpu_output[i].s[0]));
					Uint32 ig = Uint32(toInt(cpu_output[i].s[1]));
					Uint32 ib = Uint32(toInt(cpu_output[i].s[2]));
					pixels[i] = 255<<24 + ir<<16 + ig<<8 + ib;
					draw_pixel(renderer, ir, ig, ib, i%image_width, (i/image_width)%image_height);
		}
		
		memset(cpu_output, 255, image_width * image_height * sizeof(Uint32));	
		
		SDL_RenderPresent(renderer);
		while( SDL_PollEvent( &event ) ){
        switch( event.type ){
			case SDL_KEYDOWN:switch( event.key.keysym.sym ){
			case SDLK_ESCAPE: quit = true; break; 
			case SDLK_LEFT: x-=0.5f*delta_time;break;
			case SDLK_RIGHT: x+=0.5f*delta_time; break;
			case SDLK_UP: y+=0.5f*delta_time; break;
			case SDLK_DOWN:y-=0.5f*delta_time;break; 
			default:break;}}
        }
        delta_time =  ( std::clock() - begin ) / (double) CLOCKS_PER_SEC;

    }
	
		queue.finish();

	// save image
	saveImage();
	cout << "Saved image to 'opencl_raytracer.ppm'" << endl;

	// release memory
	cleanUp();

	system("PAUSE");
	return 0;
}
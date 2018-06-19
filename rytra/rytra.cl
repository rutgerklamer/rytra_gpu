__constant float EPSILON = 0.00003f; 
__constant float PI = 3.14159265359f;
__constant int SAMPLES = 128;
__constant float MAX_FLOAT = 3.402823466e+38F;

static float get_random(unsigned int *seed0, unsigned int *seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	*seed0 = 36969 * ((*seed0) & 65535) + ((*seed0) >> 16);  
	*seed1 = 18000 * ((*seed1) & 65535) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}

enum Mat { LAMB, MET };

struct Material {
	enum Mat mat;
	float3 color;
};

struct Ray {
 float3 origin;
 float3 direction;
};

struct Hit_record {
	float t;
	float3 p;
	float3 normal;
	struct Material* mat_ptr;
};

struct Sphere {
	struct Hit_record hit_record;
	float3 center;
	float radius;
};

struct Hitable_list {
	struct Sphere* spheres[5];
};

struct Camera {
	float3 lower_left_corner;
	float3 horizontal;
	float3 vertical;
	float3 origin;
};

float3 get_float(float x, float y, float z) {
		float3 w = {x,y,z};
		return w;
}

float3 reflect (float3 v, float3 n) {
		return v-2.0f*dot(v,n)*n;
}

bool has_hit_sphere(struct Ray r, float t_min, float t_max, struct Sphere* s, struct Hit_record* hr) {
		float3 oc = r.origin - s->center;
		float a = dot(r.direction, r.direction);
		float b = dot(oc,r.direction);
		float c = dot(oc,oc)-s->radius*s->radius;
		float discriminant = b*b-a*c;
		if (discriminant > 0) {
			float temp = (-b - sqrt(b*b-a*c))/a;
			if (temp < t_max && temp > t_min) {
				hr->t = temp;
				hr->p = r.origin+get_float(temp,temp,temp)*r.direction;
				hr->normal = (hr->p - s->center)/s->radius;
			    hr->mat_ptr = s->hit_record.mat_ptr;
				return true;
			}
			temp = (-b + sqrt(b*b-a*c))/a;
			if (temp < t_max && temp > t_min) {
				hr->t = temp;
				hr->p = r.origin+get_float(temp,temp,temp)*r.direction;
				hr->normal = (hr->p - s->center)/s->radius;
				hr->mat_ptr = s->hit_record.mat_ptr;
				return true;
			}

		}
		return false;
}

bool world_hit(struct Ray r, float t_min, float t_max, struct Hit_record* hr,  struct Hitable_list hl)
{
		struct Hit_record temp_rec;
		bool hit_anything = false;
		float closest_so_far = t_max;
		for (int i = 0; i < 5; i++) {
				if (has_hit_sphere(r, t_min, closest_so_far, hl.spheres[i], &temp_rec)) {
					hit_anything = true;
					closest_so_far = temp_rec.t;
					*hr = temp_rec;
				}
		}
	return hit_anything;
}

float hit_sphere(float3 center, float radius, struct Ray r, struct Hit_record hr) {
	float3 oc = r.origin - center;
	float a = dot(r.direction, r.direction);
	float b = 2.0*dot(oc,r.direction);
	float c = dot(oc,oc)-radius*radius;
	float discriminant = b*b-4*a*c;
	if (discriminant<0.0f)
		return -1.0f;
	else
		return(-b - sqrt(discriminant))/(2.0*a);
}

bool scatter_ray_metal(struct Ray r, struct Hit_record hr, float3* attenuation, struct Ray* scattered, struct Material* mats) 
{
	float3 reflected = reflect(normalize(r.direction), hr.normal);
	scattered->origin = hr.p;
	scattered->direction = reflected;
	*attenuation = mats->color;
	return (dot(reflected, hr.normal)>0);
}

bool scatter_ray_lambertian(struct Ray r, struct Hit_record hr, float3* attenuation, struct Ray* scattered, struct Material* mats, const int* seed0, const int* seed1) 
{
	float3 target = hr.p + hr.normal + normalize(2.0f*get_float(get_random(seed0,seed1), get_random(seed0,seed1), get_random(seed0,seed1)));
	scattered->origin = hr.p;
	scattered->direction = target-hr.p;
    *attenuation = mats->color;
    return true;
}


float3 get_color(struct Ray r, struct Hitable_list hl, const int* seed0, const int* seed1, int depth) {
	struct Hit_record rec;
	if (world_hit(r, 0.001, MAX_FLOAT , &rec,  hl)) {
		struct Ray scattered;
		float3 attenuation;
		if (depth < 5 && rec.mat_ptr->mat == MET && scatter_ray_metal(r, rec, &attenuation, &scattered, rec.mat_ptr)) {
			return (attenuation)*get_color(scattered,hl,seed0,seed1,depth+1);
		} else if (depth < 5 && rec.mat_ptr->mat == LAMB && scatter_ray_lambertian(r, rec, &attenuation, &scattered, rec.mat_ptr, seed0, seed1)) { 
			return (attenuation)*get_color(scattered,hl,seed0,seed1,depth+1);
		} else {
			return rec.mat_ptr->color;
		}
	} else {
		float3 unit_direction = normalize(r.direction);
		float t = 0.5*(unit_direction.y+1.0);
		return get_float((1.0-t),(1.0-t),(1.0-t))*get_float(1,1,1)+get_float(t,t,t)*get_float(0.5,0.7,1.0);
	}
}

struct Ray get_ray(struct Camera c, float u, float v)
{
	struct Ray r;
	r.origin = c.origin;
	r.direction = c.lower_left_corner + get_float(u,u,u) * c.horizontal + get_float(v,v,v) * c.vertical - c.origin;
	return r;
}


__kernel void render_kernel(__global float3* output, int width, int height, int randss, float ysphere, float xsphere)
{
	const int work_item_id = get_global_id(0); 
	int x = work_item_id % width; 
	int y = height - work_item_id / width % height; 
	
	float u = (float)x / (float)width;
	float v = (float)y / (float)height;
 	struct Camera cam = { get_float(-2.0,-1.0,-1.0), get_float(4.0,0.0,0.0), get_float(0.0,2.0,0.0), get_float(0.0,0.0,0.0) };
	
	unsigned int seed0 = x *randss;
	unsigned int seed1 = y *randss;
	
	struct Ray ray = get_ray(cam, u, v);
	struct Material red = { LAMB, get_float(0.8,0.6,0.2)} ;
	struct Material blue = { MET, get_float(0.8,0.8,0.8)} ;
	struct Hit_record hit_record_red = { 0.0f, get_float(0.0f,0.0f,0.0f), get_float(0.0f,0.0f,0.0f), &red};
	struct Hit_record hit_record_blue = { 0.0f, get_float(0.0f,0.0f,0.0f), get_float(0.0f,0.0f,0.0f), &blue};
	
	struct Sphere sphere = {hit_record_red, get_float(xsphere,ysphere,-1.0f),0.5f};
	
	struct Sphere sphere2 = {hit_record_blue, get_float(-1.0f,-0.5f,-1.0f),0.5f};
	struct Sphere sphere3 = {hit_record_blue, get_float(0,ysphere-1.0,-1.0f),0.5f};
	
	struct Sphere sphere4 = {hit_record_blue, get_float(0.0f,1.0f,-1.0f),0.5f};
	struct Sphere sphere5 = {hit_record_red, get_float(xsphere+1.0,ysphere,-1.0f),0.5f};
	
	struct Hitable_list hitable_list;
	hitable_list.spheres[0] = &sphere;
	hitable_list.spheres[1] = &sphere2;
	hitable_list.spheres[2] = &sphere3;
	hitable_list.spheres[3] = &sphere4;
	hitable_list.spheres[4] = &sphere5;

	int samples = 10;
	float3 col = {0,0,0};
	for (int s = 0; s < samples; s++) {
			col += get_color(ray, hitable_list, &seed0, &seed1, 0);
			seed0+=randss;
			seed1-=randss;
	}
	col /= samples;
	col = get_float(sqrt(col.x), sqrt(col.y), sqrt(col.z));
	
	output[work_item_id] = col; 
}


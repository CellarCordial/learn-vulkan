#include "bounds.h"
#include "vector.h"
#include <algorithm>
#include <vector>

namespace fantasy 
{
    Bounds3F create_aabb(const std::vector<float3>& position)
    {
        const auto XMinMax = std::minmax_element(
            position.begin(),
            position.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.x < rhs.x;    
            }
        );
        const auto YMinMax = std::minmax_element(
            position.begin(),
            position.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.y < rhs.y;    
            }
        );
        const auto ZMinMax = std::minmax_element(
            position.begin(),
            position.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.z < rhs.z;    
            }
        );
        
        const float3 min(
            XMinMax.first->x,
            YMinMax.first->y,
            ZMinMax.first->z
        );
        
        // max - Center相当于(min + max) / 2
        const float3 max(
            XMinMax.second->x,
            YMinMax.second->y,
            ZMinMax.second->z
        );    

        return Bounds3F{ min, max };
    }

    Circle merge(const Circle& circle0, const Circle& circle1)
	{
        float2 center0_to_1 = circle0.center - circle1.center;
        float center_distance = center0_to_1.length();
        if (circle0.radius - circle1.radius >= center_distance) return circle0;
        if (circle1.radius - circle0.radius >= center_distance) return circle1;

        float radius = (center_distance + circle0.radius + circle1.radius) * 0.5f;
        float2 center = circle0.center + (center0_to_1 / center_distance) * (radius - circle0.radius);
		return Circle(center, radius);
	}
	
	Sphere merge(const Sphere& sphere0, const Sphere& sphere1)
	{
        float3 center0_to_1 = sphere0.center - sphere1.center;
        float center_distance = center0_to_1.length();
        if (sphere0.radius - sphere1.radius >= center_distance) return sphere0;
        if (sphere1.radius - sphere0.radius >= center_distance) return sphere1;

        float radius = (center_distance + sphere0.radius + sphere1.radius) * 0.5f;
        float3 center = sphere0.center + (-center0_to_1 / center_distance) * (radius - sphere0.radius);
		return Sphere(center, radius);
	}

	Sphere merge(const std::vector<Sphere>& spheres)
    {
        uint32_t min_idx[3] = {};
        uint32_t max_idx[3] = {};
        for (uint32_t i = 0; i < spheres.size(); i++) 
        {
            for (uint32_t k = 0; k < 3; k++) 
            {
                if (spheres[i].center[k] - spheres[i].radius < spheres[min_idx[k]].center[k] - spheres[min_idx[k]].radius) min_idx[k] = i;
                if (spheres[i].center[k] + spheres[i].radius < spheres[max_idx[k]].center[k] + spheres[max_idx[k]].radius) max_idx[k] = i;
            }
        }

        float max_len = 0;
        uint32_t max_axis = 0;
        for (uint32_t k = 0; k < 3; k++) 
        {
            Sphere spmin = spheres[min_idx[k]];
            Sphere spmax = spheres[max_idx[k]];
            float tlen = float3(spmax.center - spmin.center).length() + spmax.radius + spmin.radius;
            if (tlen > max_len) max_len = tlen, max_axis = k;
        }

        Sphere sphere = spheres[min_idx[max_axis]];
        sphere = merge(sphere, spheres[max_idx[max_axis]]);
        for (uint32_t i = 0; i < spheres.size(); i++) 
        {
            sphere = merge(sphere, spheres[i]);
        }
        for (uint32_t i = 0; i < spheres.size(); i++) 
        {
            float radius_diff = sphere.radius - spheres[i].radius;
            float t1 = radius_diff * radius_diff;
            float t2 = float3(sphere.center - spheres[i].center).length_squared();
            assert(t1 + 1e-6 >= t2);
        }
        return sphere;
    }
}
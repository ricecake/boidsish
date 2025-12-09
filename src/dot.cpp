#include "dot.h"

#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "graphics.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <shader.h>

namespace Boidsish {

	Dot::Dot(int id, float x, float y, float z, float size, float r, float g, float b, float a, int trail_length) {
		this->id = id;
		this->x = x;
		this->y = y;
		this->z = z;
		this->size = size;
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
		this->trail_length = trail_length;
	}

	void Dot::render() const {
		float radius = size * 0.01f;

		GLfloat material_ambient[] = {r * 0.2f, g * 0.2f, b * 0.2f, a};
		GLfloat material_diffuse[] = {r, g, b, a};
		GLfloat material_specular[] = {0.5f, 0.5f, 0.5f, a};
		GLfloat material_shininess[] = {32.0f};

		glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, material_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, material_shininess);

		const int longitude_segments = 12;
		const int latitude_segments = 8;

		glBegin(GL_TRIANGLES);

		for (int lat = 0; lat < latitude_segments; ++lat) {
			float lat0 = M_PI * (-0.5f + (float)lat / latitude_segments);
			float lat1 = M_PI * (-0.5f + (float)(lat + 1) / latitude_segments);

			float y0 = sin(lat0) * radius;
			float y1 = sin(lat1) * radius;
			float r0 = cos(lat0) * radius;
			float r1 = cos(lat1) * radius;

			for (int lon = 0; lon < longitude_segments; ++lon) {
				float lon0 = 2 * M_PI * (float)lon / longitude_segments;
				float lon1 = 2 * M_PI * (float)(lon + 1) / longitude_segments;

				float x0 = cos(lon0);
				float z0 = sin(lon0);
				float x1 = cos(lon1);
				float z1 = sin(lon1);

				glNormal3f(x0 * cos(lat0), sin(lat0), z0 * cos(lat0));
				glVertex3f(x0 * r0, y0, z0 * r0);

				glNormal3f(x1 * cos(lat0), sin(lat0), z1 * cos(lat0));
				glVertex3f(x1 * r0, y0, z1 * r0);

				glNormal3f(x1 * cos(lat1), sin(lat1), z1 * cos(lat1));
				glVertex3f(x1 * r1, y1, z1 * r1);

				glNormal3f(x0 * cos(lat0), sin(lat0), z0 * cos(lat0));
				glVertex3f(x0 * r0, y0, z0 * r0);

				glNormal3f(x1 * cos(lat1), sin(lat1), z1 * cos(lat1));
				glVertex3f(x1 * r1, y1, z1 * r1);

				glNormal3f(x0 * cos(lat1), sin(lat1), z0 * cos(lat1));
				glVertex3f(x0 * r1, y1, z0 * r1);
			}
		}

		glEnd();
	}
} // namespace Boidsish
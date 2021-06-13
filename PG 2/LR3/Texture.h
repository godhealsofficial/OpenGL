#ifndef TEXTURE_H
#define TEXTURE_H

#include "common.h"
#include "ShaderProgram.h"

struct Texture2D
{
	Texture2D(GLenum format, GLsizei width, GLsizei height, const void* data);
	explicit Texture2D(const std::string& file_name);

	~Texture2D();

	GLuint GetColorTexId() const { return tex; }
protected:

	GLuint tex;
};

void bindTexture(const ShaderProgram& program, int unit, const std::string& name, Texture2D& texture);


#endif
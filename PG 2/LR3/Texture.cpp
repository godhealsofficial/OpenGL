#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


Texture2D::Texture2D(GLenum format, GLsizei width, GLsizei height, const void* data)
{
	glGenTextures(1, &tex); GL_CHECK_ERRORS;
	glBindTexture(GL_TEXTURE_2D, tex); GL_CHECK_ERRORS;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);  GL_CHECK_ERRORS;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); GL_CHECK_ERRORS;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  GL_CHECK_ERRORS;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);  GL_CHECK_ERRORS;

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data); GL_CHECK_ERRORS;
	glGenerateMipmap(GL_TEXTURE_2D);
}


Texture2D::Texture2D(const std::string& filename)
{
  int w;
  int h;
  int nComponents;
  unsigned char* imageData = stbi_load(filename.c_str(), &w, &h, &nComponents, STBI_rgb);

  if (imageData )
  {
    GLenum format = GL_RGB;
    if (nComponents == 1)
      format = GL_RED;
    else if (nComponents == 3)
      format = GL_RGB;
    else if (nComponents == 4)
      format = GL_RGBA;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, imageData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(imageData);
  }
  else
  {
    std::cout << "Failed to load texture: " << filename << std::endl;
    stbi_image_free(imageData);
  }

}

Texture2D::~Texture2D()
{
	glDeleteTextures(1, &tex);
	tex = -1;
}


//функция для привязки текстуры
//делает активной созданную текстуру, а затем привязывает её к имени в шейдерной программе
void bindTexture(const ShaderProgram& program, int unit, const std::string& name, Texture2D& texture)
{
	glActiveTexture(GL_TEXTURE0 + unit);  GL_CHECK_ERRORS;
	glBindTexture(GL_TEXTURE_2D, texture.GetColorTexId());  GL_CHECK_ERRORS;

	program.SetUniform(name, unit); GL_CHECK_ERRORS;
}

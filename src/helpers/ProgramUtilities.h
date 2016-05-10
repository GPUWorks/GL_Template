#ifndef ProgramUtilities_h
#define ProgramUtilities_h

#include <GL/glew.h>
#include <string>

/// This macro is used to check for OpenGL errors with access to the file and line number where the error is detected.
#define checkGLError() _checkGLError(__FILE__, __LINE__)

/// Converts a GLenum error number into a human-readable string.
std::string getGLErrorString(GLenum error);

/// Check if any OpenGL error has been detected and log it.
int _checkGLError(const char *file, int line);

/// Return the content of a text file at the given path, as a string.
std::string loadStringFromFile(const std::string & path);

/// Load a shader of the given type from a string
GLuint loadShader(const std::string & prog, GLuint type);

/// create a GLProgram using the hader code contained in the given files.
GLuint createGLProgram(const std::string & vertexPath, const std::string & fragmentPath, const std::string & geometryPath = "");


#endif
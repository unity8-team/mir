#ifndef SHADER_UTIL_H
#define SHADER_UTIL_H



struct uniform_info
{
   const char *name;
   GLuint size;  /**< number of value[] elements: 1, 2, 3 or 4 */
   GLenum type;  /**< GL_FLOAT, GL_FLOAT_VEC4, GL_INT, etc */
   GLfloat value[4];
   GLint location;  /**< filled in by InitUniforms() */
};

#define END_OF_UNIFORMS   { NULL, 0, GL_NONE, { 0, 0, 0, 0 }, -1 }


struct attrib_info
{
   const char *name;
   GLuint size;  /**< number of value[] elements: 1, 2, 3 or 4 */
   GLenum type;  /**< GL_FLOAT, GL_FLOAT_VEC4, GL_INT, etc */
   GLint location;
};


extern GLboolean
ShadersSupported(void);

extern GLuint
CompileShaderText(GLenum shaderType, const char *text);

extern GLuint
CompileShaderFile(GLenum shaderType, const char *filename);

extern GLuint
LinkShaders(GLuint vertShader, GLuint fragShader);

extern GLboolean
ValidateShaderProgram(GLuint program);

extern GLdouble
GetShaderCompileTime(void);

extern GLdouble
GetShaderLinkTime(void);

extern void
SetUniformValues(GLuint program, struct uniform_info uniforms[]);

extern GLuint
GetUniforms(GLuint program, struct uniform_info uniforms[]);

extern void
PrintUniforms(const struct uniform_info uniforms[]);

extern GLuint
GetAttribs(GLuint program, struct attrib_info attribs[]);

extern void
PrintAttribs(const struct attrib_info attribs[]);

/* These pointers are only valid after calling ShadersSupported.
 */
extern PFNGLCREATESHADERPROC CreateShader;
extern PFNGLDELETESHADERPROC DeleteShader;
extern PFNGLSHADERSOURCEPROC ShaderSource;
extern PFNGLGETSHADERIVPROC GetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC CreateProgram;
extern PFNGLDELETEPROGRAMPROC DeleteProgram;
extern PFNGLATTACHSHADERPROC AttachShader;
extern PFNGLLINKPROGRAMPROC LinkProgram;
extern PFNGLUSEPROGRAMPROC UseProgram;
extern PFNGLGETPROGRAMIVPROC GetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
extern PFNGLVALIDATEPROGRAMARBPROC ValidateProgramARB;
extern PFNGLUNIFORM1IPROC Uniform1i;
extern PFNGLUNIFORM1FVPROC Uniform1fv;
extern PFNGLUNIFORM2FVPROC Uniform2fv;
extern PFNGLUNIFORM3FVPROC Uniform3fv;
extern PFNGLUNIFORM4FVPROC Uniform4fv;
extern PFNGLGETACTIVEATTRIBPROC GetActiveAttrib;
extern PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;

#endif /* SHADER_UTIL_H */

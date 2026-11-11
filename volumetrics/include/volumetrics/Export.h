#pragma once

#ifdef _WIN32
# ifdef VOLUMETRIC_CPU_EXPORTS
#  define VOLUMETRIC_CPU_API __declspec(dllexport)
# else
#  define VOLUMETRIC_CPU_API __declspec(dllimport)
# endif
# ifdef VOLUMETRIC_FOG_EXPORTS
#  define VOLUMETRIC_FOG_API __declspec(dllexport)
# else
#  define VOLUMETRIC_FOG_API __declspec(dllimport)
# endif
#else
# define VOLUMETRIC_CPU_API __attribute__((visibility("default")))
# define VOLUMETRIC_FOG_API __attribute__((visibility("default")))
#endif

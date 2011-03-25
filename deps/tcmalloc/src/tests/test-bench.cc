#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <assert.h>

#include "google\malloc_extension.h"

class HRTimer {
  public:
    inline HRTimer(const char* _msg) {
      if (_msg)
        msg = _msg;
      LARGE_INTEGER _freq;
      if (!::QueryPerformanceFrequency(&_freq))
        _ASSERTE(false);
      freq = (double)_freq.QuadPart;
      if (!::QueryPerformanceCounter(&start))
        _ASSERTE(false);
    }

    inline ~HRTimer() {
      if (!::QueryPerformanceCounter(&stop))
        _ASSERTE(false);
      double delta = (stop.QuadPart - start.QuadPart) / (freq / 1000.0);
      std::cout << std::flush << std::setw(6) << ::GetCurrentThreadId() << " " << msg << " " << std::setprecision(3) << delta << "ms" << std::endl;
    }

private:

    LARGE_INTEGER start;
    LARGE_INTEGER stop;
    double freq;
    std::string msg;
};

static bool test1()

{

   const int n = 10000;

   void *buffers[n];

   static unsigned int sizes[] = {

      1, 2, 3, 4, 8, 12, 16, 24, 32, 48, 64, 80, 96, 128, 160, 256,

      340, 512, 640, 768, 1024, 1600, 2048, 3084, 4096 };

   for ( int loop = 0; loop < sizeof(sizes)/sizeof(unsigned int); ++loop )

   {

      unsigned int size = sizes[loop];

      for ( int loop2 =0; loop2 < 10; ++loop2 )

      {

         for ( int index =0; index < n; ++index )

         {

            buffers[index] = malloc( size );

         }

 

         for ( int index =0; index < n; ++index )

         {

            free( buffers[index] );

         }

      }

   }

   return true;

}

static bool test2()

{

   const int n = 10000;

   unsigned char *buffers[n];

   static unsigned int sizes[] = {

      1, 2, 3, 4, 8, 12, 16, 24, 32, 48, 64, 80, 96, 128, 160, 256,

      340, 512, 640, 768, 1024, 1600, 2048, 3084, 4096 };

   unsigned char serial = 0;

   for ( int loop = 0; loop < sizeof(sizes)/sizeof(unsigned int); ++loop )

   {

      unsigned int size = sizes[loop];

      for ( int loop2 =0; loop2 < 10; ++loop2 )

      {

         serial += 1;

         for ( int index =0; index < n; ++index )

         {

            unsigned char *buffer = static_cast<unsigned char *>( malloc( size ) );

            buffers[index] = buffer;

            memset( buffer, serial + index, size );

         }

 

         for ( int index =0; index < n; ++index )

         {

            unsigned char *buffer = buffers[index];

            unsigned char expected = serial + index;

            for ( unsigned int offset = 0; offset < size; ++offset )

            {

               if ( buffer[offset] != expected )

               {

                  printf( "test2 failed: loop = %d, loop2 = %d, index = %d, size = %d\n",

                          loop, loop2, index, size );

                  return false;

               }

 

            }

            free( buffer );

         }

      }

   }

   return true;

}

int main(int argc, char** argv) {
  {
    HRTimer t("test1");
    test1();
  }
  {
    HRTimer t("test2");
    test2();
  }
  MallocExtension::instance()->ReleaseFreeMemory();
  return 0;
}

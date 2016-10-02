//
// lz4.h
//

#ifndef LZ4LIB_HH
#define LZ4LIB_HH

#include <memory>
#include <string>

/**
 * \namespace leap::crypto
 * \brief Crypto Routines
 *
**/

namespace leap { namespace crypto
{

  //-------------------------- LZ4 --------------------------------------------
  //---------------------------------------------------------------------------

  size_t lz4_compress(const void *source, void *dest, size_t sourcesize, size_t maxdestsize);
  size_t lz4_compress(const void *source, void *dest, size_t *sourcesizeptr, size_t targetdestsize);

  size_t lz4_decompress(const void *source, void *dest, size_t compressedsize, size_t maxdecompressedsize);

} } // namespace crypto

#endif // LZ4LIB_HH

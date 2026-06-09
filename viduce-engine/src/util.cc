#include "engine/util.h"

#include <string>

extern "C" {
#include <libavutil/error.h>
}  // extern "C"

namespace viduce::engine::util {

std::string AvErrToStr(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  return std::string(av_make_error_string(errbuf, sizeof(errbuf), errnum));
}

}  // namespace viduce::engine::util

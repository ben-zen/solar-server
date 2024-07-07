#pragma once

#include <memory>

#include <curl/curl.h>


class curl_handle {
private:
  std::shared_ptr<CURL> m_ptr;

public:
  curl_handle();
  curl_handle(CURL *curl);
  curl_handle(curl_handle &other);

  operator CURL*() const;

  static curl_handle create_handle();
};


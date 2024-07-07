#include "curl.hh"

curl_handle::curl_handle() {}

auto curl_deleter = [](auto *p) {
  curl_easy_cleanup(p);
};

curl_handle::curl_handle(CURL *curl) 
  : m_ptr(curl, curl_deleter) {

}

curl_handle::curl_handle(curl_handle &other) 
  : m_ptr(other.m_ptr) {
  
}

curl_handle::operator CURL* () const {
  return m_ptr.get();
}

curl_handle curl_handle::create_handle() {
  CURL *curl = curl_easy_init();
  curl_handle handle{curl};
  curl = nullptr;

  return handle;
}
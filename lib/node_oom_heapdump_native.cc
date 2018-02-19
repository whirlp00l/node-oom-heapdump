#include <nan.h>
#include <v8-profiler.h>
#include <stdlib.h>
#if defined(_WIN32)
  #include <time.h>
  #define snprintf _snprintf
#else
  #include <sys/time.h>
#endif

using namespace v8;

char filename[256];
bool addTimestamp;

char* ToCString(Local<String> str) {
  String::Utf8Value value(str);
  return *value ? *value : "<string conversion failed>";
}

class FileOutputStream: public OutputStream {
  public:
    FileOutputStream(FILE* stream): stream_(stream) { }
    virtual int GetChunkSize() {
      return 65536;
    }
    virtual void EndOfStream() { }
    virtual WriteResult WriteAsciiChunk(char* data, int size) {
      const size_t len = static_cast<size_t>(size);
      size_t off = 0;
      while (off < len && !feof(stream_) && !ferror(stream_))
        off += fwrite(data + off, 1, len - off, stream_);
      return off == len ? kContinue : kAbort;
    }

  private:
    FILE* stream_;
};

void OnOOMError(const char *location, bool is_heap_oom) {
  if (addTimestamp) {
    // Add timestamp to filename
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char * pch;
    pch = strstr (filename,".heapsnapshot");
    strncpy (pch,"",1);
    strcat (filename, "-%Y%m%dT%H%M%S.heapsnapshot");
    
    char newFilename[256];
    strftime(newFilename, sizeof(filename), filename, timeinfo);
    strcpy(filename, newFilename);
  }

  fprintf(stderr, "Generating Heapdump to '%s' now...\n", filename);
  FILE* fp = fopen(filename, "w");
  if (fp == NULL) abort();

  // Create heapdump, depending on which Node.js version this can differ
  // see https://github.com/trevnorris/node-ofe/blob/master/ofe.cc
  #if NODE_VERSION_AT_LEAST(0, 11, 13)
    Isolate* isolate = Isolate::GetCurrent();
  #if NODE_VERSION_AT_LEAST(3, 0, 0)
    const HeapSnapshot* snap = isolate->GetHeapProfiler()->TakeHeapSnapshot();
  #else
    const HeapSnapshot* snap = isolate->GetHeapProfiler()->TakeHeapSnapshot(String::Empty(isolate));
  #endif
  #else
    const HeapSnapshot* snap = HeapProfiler::TakeSnapshot(String::Empty());
  #endif
  
  FileOutputStream stream(fp);
  snap->Serialize(&stream, HeapSnapshot::kJSON);
  fclose(fp);

  fprintf(stderr, "Done! Exiting process now.\n");
  exit(1);
}

void ParseArgumentsAndSetErrorHandler(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  isolate->SetOOMErrorHandler(OnOOMError);
  
  // parse JS arguments
  // 1: filename
  // 2: addTimestamp boolean
  strncpy(filename, ToCString(args[0]->ToString()), sizeof(filename) - 1);
  addTimestamp = args[1]->BooleanValue();
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "call", ParseArgumentsAndSetErrorHandler);
}

NODE_MODULE(NODE_OOM_HEAPDUMP_NATIVE, init)
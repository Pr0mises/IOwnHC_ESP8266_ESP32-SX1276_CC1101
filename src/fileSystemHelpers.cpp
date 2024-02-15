#include <fileSystemHelpers.h>

void printFileInfo(const char* dirName, const char* filePath, uint8_t level) {
  File file = LittleFS.open((String(dirName) + String(filePath)).c_str(), "r");
  if (!file.isDirectory()) {
    std::string depth((level), '\t');
    Serial.printf("%s", depth.c_str());
    Serial.print(filePath);
    Serial.print("\t\t");
    Serial.println(file.size());
  }
  file.close();
}

void traverseDirectory(const char* dirName, uint8_t level) {

#if defined(ESP8266)    
  Dir dir = LittleFS.openDir(dirName);

  while (dir.next()) {
    String fileName = dir.fileName();
    if (dir.isDirectory()) {
      // If the entry is a directory, traverse it recursively
        std::string depth((level), '\t');
        Serial.printf("%s", depth.c_str());
      Serial.printf("%s\n", fileName.c_str());
      traverseDirectory((String(dirName) + "/" + fileName).c_str(), level+1);
    } else {
      // If the entry is a file, print its information
      printFileInfo(dirName, fileName.c_str(), level);
    }
  }
#elif defined(ESP32)
  File root = LittleFS.open(dirName);
  File fileName;
  while (fileName = root.openNextFile()){
    if(fileName.isDirectory()){
      std::string depth((level), '\t');
      Serial.printf("%s", depth.c_str());
      Serial.printf("%s\n", fileName.name());
      traverseDirectory((String(dirName) + "/" + fileName).c_str(), level+1);
    }else{
      printFileInfo(dirName, fileName.name(), level);
    }
  }

    
#endif
}

void listFS() {
    traverseDirectory("/", 0);
}

void cat(const char *fname) {
    if (!LittleFS.exists(fname)) {
      Serial.printf("File %s does not exists\n\n", fname);
      return;
    }
    File file = LittleFS.open(fname, "r");
    String record = file.readString();
    Serial.printf("%s\n", record.c_str());
    file.close();
}

void rm(const char *fname) {
    if (!LittleFS.exists(fname)) {
      Serial.printf("File %s does not exists\n\n", fname);
      return;
    }
    LittleFS.remove(fname);
}
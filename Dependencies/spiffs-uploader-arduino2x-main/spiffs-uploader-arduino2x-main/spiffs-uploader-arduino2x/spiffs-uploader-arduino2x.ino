#include "SPIFFS.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 SPIFFS Text File Reader");
  Serial.println("=============================");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  Serial.println("SPIFFS mounted successfully!\n");
  
  // List all files first
  Serial.println("=== Files in SPIFFS ===");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file) {
    Serial.printf("FILE: %s, SIZE: %d bytes\n", file.name(), file.size());
    file = root.openNextFile();
  }
  root.close();
  
  Serial.println("\n=== Reading Text Files ===");
  
  // Read all text files automatically
  File rootDir = SPIFFS.open("/");
  File currentFile = rootDir.openNextFile();
  
  while(currentFile) {
    String fileName = String(currentFile.name());
    
    // Check if it's a text file (you can add more extensions)
    if (fileName.endsWith(".txt") || 
        fileName.endsWith(".html") || 
        fileName.endsWith(".css") || 
        fileName.endsWith(".js") || 
        fileName.endsWith(".json") ||
        fileName.endsWith(".csv") ||
        fileName.endsWith(".xml") ||
        fileName.endsWith(".log")) {
      
      Serial.printf("\n--- Content of %s ---\n", fileName.c_str());
      
      // Read and display file content
      currentFile.seek(0); // Go to beginning of file
      while(currentFile.available()) {
        Serial.write(currentFile.read());
      }
      
      Serial.println("\n--- End of file ---");
    }
    
    currentFile = rootDir.openNextFile();
  }
  rootDir.close();
  
  Serial.println("\n=== All files read! ===");
}

void loop() {
  // You can add interactive commands here if needed
  
  // Example: Read a specific file when user sends 'r' via Serial Monitor
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.equals("r")) {
      // Re-read all files
      setup();
    } else if (command.startsWith("read ")) {
      // Read specific file: "read filename.txt"
      String filename = command.substring(5);
      
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      
      File file = SPIFFS.open(filename.c_str(), "r");
      if (file) {
        Serial.printf("\n--- Content of %s ---\n", filename.c_str());
        while(file.available()) {
          Serial.write(file.read());
        }
        Serial.println("\n--- End of file ---");
        file.close();
      } else {
        Serial.printf("File not found: %s\n", filename.c_str());
      }
    } else if (command.equals("list")) {
      // List files
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      Serial.println("\nFiles in SPIFFS:");
      while(file) {
        Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
      }
      root.close();
    } else if (command.equals("help")) {
      Serial.println("\nCommands:");
      Serial.println("  r - Re-read all files");
      Serial.println("  list - List all files");
      Serial.println("  read <filename> - Read specific file");
      Serial.println("  help - Show this help");
    }
  }
  
  delay(100);
}
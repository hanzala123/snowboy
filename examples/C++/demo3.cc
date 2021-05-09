#include "include/snowboy-detect.h"
#include <iostream>
#include <string.h>

using namespace std;

#define BUFFER_SIZE 2000

/**
 * run like this:
 * rec -q -r 16000 -c 1 -b 16 -e signed-integer -t wav - | ./demo3
 */
int main(int argc, char* argv[]) {
  snowboy::SnowboyDetect detector("resources/common.res", "resources/models/snowboy.umdl");
  detector.SetSensitivity("0.5");
  detector.SetAudioGain(1.0);
  detector.ApplyFrontend(false);

  short data_buffer[BUFFER_SIZE];

  int mode = 0;
  while (1) {
    cin.read((char*)&data_buffer[0], BUFFER_SIZE*2);

    int newMode = detector.RunDetection(&data_buffer[0], BUFFER_SIZE);
    if (newMode != mode) {
      string str;
      switch (newMode) {
        case -2: str = "silence"; break;
        case -1: str = "error"; break;
        case  0: str = "noise"; break;
        default: str = "hotword"; break;
      }

      mode = newMode;
      cout << str << endl;
    }
  }
}

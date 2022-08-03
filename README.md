# ESP32 CAM WIFI TIMELAPSE CAMERA

This proof of concept uses an ESP32 CAM module, with an SD card. Takes a picture, an upload it to a server via WiFI using multipart HTTP POST. The cam stays in deep sleep between takes

For the sake of simplicity no authentication method was implemented and a lot of failed cases aren't handled properly. Use it as a starting point for more complex and robust projects


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define paths for FFmpeg and FFprobe (relative to the executable)
#define FFMPEG_PATH "bin\\ffmpeg.exe"
#define FFPROBE_PATH "bin\\ffprobe.exe"
#define LOG_FILE "C:\\Users\\%USERNAME%\\AppData\\Local\\Temp\\quickpress.log"

// Function to get the username for the log file path
char *get_username()
{
  char *username = getenv("USERNAME");
  if (username == NULL)
  {
    fprintf(stderr, "Failed to get username\n");
    exit(1);
  }
  return username;
}

// Function to expand the log file path with the username
char *expand_log_path()
{
  char *username = get_username();
  char *log_path = (char *)malloc(512 * sizeof(char));
  if (log_path == NULL)
  {
    fprintf(stderr, "Memory allocation failed for log path\n");
    exit(1);
  }
  snprintf(log_path, 512, "C:\\Users\\%s\\AppData\\Local\\Temp\\quickpress.log", username);
  return log_path;
}

// Function to log messages
void log_message(const char *message)
{
  char *log_path = expand_log_path();
  FILE *log_file = fopen(log_path, "a");
  if (log_file == NULL)
  {
    fprintf(stderr, "Failed to open log file: %s\n", log_path);
    free(log_path);
    return;
  }
  fprintf(log_file, "%s\n", message);
  fclose(log_file);
  free(log_path);
}

// Function to get the duration of the video using FFprobe
double get_video_duration(const char *file_path)
{
  char command[1024];
  snprintf(command, sizeof(command), "\"%s\" -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", FFPROBE_PATH, file_path);

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE; // Hide the console window

  HANDLE pipe_read, pipe_write;
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
  if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0))
  {
    log_message("Failed to create pipe for FFprobe");
    exit(1);
  }

  si.hStdOutput = pipe_write;
  si.hStdError = pipe_write;
  si.dwFlags |= STARTF_USESTDHANDLES;

  if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
  {
    log_message("Failed to execute FFprobe");
    CloseHandle(pipe_read);
    CloseHandle(pipe_write);
    exit(1);
  }

  CloseHandle(pipe_write);

  char buffer[128];
  DWORD bytes_read;
  char output[128] = {0};
  while (ReadFile(pipe_read, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0)
  {
    buffer[bytes_read] = '\0';
    strncat(output, buffer, bytes_read);
  }

  CloseHandle(pipe_read);
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exit_code != 0)
  {
    log_message("FFprobe returned non-zero exit code");
    exit(1);
  }

  double duration = atof(output);
  if (duration <= 0)
  {
    log_message("Invalid video duration");
    exit(1);
  }

  char log_msg[256];
  snprintf(log_msg, sizeof(log_msg), "Video duration: %f seconds", duration);
  log_message(log_msg);
  return duration;
}

// Function to generate a unique output filename
char *generate_output_filename(const char *file_path)
{
  char *base = strdup(file_path);
  if (base == NULL)
  {
    log_message("Memory allocation failed for output filename");
    exit(1);
  }

  char *ext = strrchr(base, '.');
  if (ext == NULL)
  {
    log_message("File has no extension");
    free(base);
    exit(1);
  }

  *ext = '\0'; // Remove the extension
  ext++;       // Point to the original extension

  char *output_file = (char *)malloc(512 * sizeof(char));
  if (output_file == NULL)
  {
    log_message("Memory allocation failed for output filename");
    free(base);
    exit(1);
  }

  snprintf(output_file, 512, "%s_compressed.%s", base, ext);
  int i = 1;
  while (GetFileAttributes(output_file) != INVALID_FILE_ATTRIBUTES)
  {
    snprintf(output_file, 512, "%s_compressed%d.%s", base, i, ext);
    i++;
  }

  free(base);
  return output_file;
}

// Function to compress the video
void compress_video(const char *file_path, double target_size_mb)
{
  double duration = get_video_duration(file_path);

  // Calculate bitrates for the target size
  double target_size_bytes = target_size_mb * 1024 * 1024;   // Convert MB to bytes
  double audio_bitrate = 128000;                             // Fixed audio bitrate in bits per second
  double total_bitrate = (target_size_bytes * 8) / duration; // Total bitrate in bps
  double video_bitrate = total_bitrate - audio_bitrate;      // Video bitrate in bps

  if (video_bitrate <= 0)
  {
    log_message("Desired size is too small");
    exit(1);
  }

  char *output_file = generate_output_filename(file_path);
  char command[2048];
  snprintf(command, sizeof(command),
           "\"%s\" -i \"%s\" -b:v %d -b:a %d -vcodec libx264 -acodec aac -pix_fmt yuv420p -color_range tv -colorspace bt709 -y \"%s\"",
           FFMPEG_PATH, file_path, (int)video_bitrate, (int)audio_bitrate, output_file);

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE; // Hide the console window

  if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
  {
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Failed to execute FFmpeg: %s", command);
    log_message(log_msg);
    free(output_file);
    exit(1);
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exit_code != 0)
  {
    log_message("FFmpeg returned non-zero exit code");
    free(output_file);
    exit(1);
  }

  char log_msg[512];
  snprintf(log_msg, sizeof(log_msg), "Compression successful: %s", output_file);
  log_message(log_msg);
  free(output_file);
}

int main(int argc, char *argv[])
{
  log_message("Script started");

  // Check if file path and target size were provided
  if (argc < 3)
  {
    log_message("Missing arguments: file path and target size required");
    return 1;
  }

  const char *file_path = argv[1];
  char *size_arg = argv[2];

  char log_msg[512];
  snprintf(log_msg, sizeof(log_msg), "File path received: %s", file_path);
  log_message(log_msg);
  snprintf(log_msg, sizeof(log_msg), "Size argument: %s", size_arg);
  log_message(log_msg);

  // Verify the file exists
  if (GetFileAttributes(file_path) == INVALID_FILE_ATTRIBUTES)
  {
    log_message("File does not exist");
    return 1;
  }

  // Get the program's directory
  char base_path[MAX_PATH];
  GetModuleFileName(NULL, base_path, MAX_PATH);
  char *last_slash = strrchr(base_path, '\\');
  if (last_slash)
  {
    *last_slash = '\0'; // Remove the executable name
  }

  // Define paths for FFmpeg and FFprobe
  char ffmpeg_path[MAX_PATH];
  char ffprobe_path[MAX_PATH];
  snprintf(ffmpeg_path, sizeof(ffmpeg_path), "%s\\%s", base_path, FFMPEG_PATH);
  snprintf(ffprobe_path, sizeof(ffprobe_path), "%s\\%s", base_path, FFPROBE_PATH);

  snprintf(log_msg, sizeof(log_msg), "FFmpeg path: %s", ffmpeg_path);
  log_message(log_msg);
  snprintf(log_msg, sizeof(log_msg), "FFprobe path: %s", ffprobe_path);
  log_message(log_msg);

  // Check if FFmpeg exists and is runnable
  if (GetFileAttributes(ffmpeg_path) == INVALID_FILE_ATTRIBUTES)
  {
    log_message("FFmpeg does not exist");
    return 1;
  }
  char command[1024];
  snprintf(command, sizeof(command), "\"%s\" -version", ffmpeg_path);
  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
  {
    log_message("Failed to run FFmpeg");
    return 1;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  if (exit_code != 0)
  {
    log_message("FFmpeg returned non-zero exit code");
    return 1;
  }

  // Check if FFprobe exists and is runnable
  if (GetFileAttributes(ffprobe_path) == INVALID_FILE_ATTRIBUTES)
  {
    log_message("FFprobe does not exist");
    return 1;
  }
  snprintf(command, sizeof(command), "\"%s\" -version", ffprobe_path);
  if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
  {
    log_message("Failed to run FFprobe");
    return 1;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  if (exit_code != 0)
  {
    log_message("FFprobe returned non-zero exit code");
    return 1;
  }

  // Handle the size argument
  double target_size_mb = atof(size_arg);
  if (target_size_mb <= 0)
  {
    log_message("Target size must be a positive number");
    return 1;
  }

  compress_video(file_path, target_size_mb);
  return 0;
}
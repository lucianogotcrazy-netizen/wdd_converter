#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH 1024
#define BUFFER_SIZE 4096

// Downloads the latest .tar.xz package from the correct mmoyles87 repository
int download_latest_plugin() {
    char download_url[MAX_PATH] = {0};
    const char *target_archive = "kde6-dynamic-wallpaper-latest.tar.xz";
    
    printf("Connecting to GitHub API to check for updates...\n");

    const char *cmd = "curl -s https://api.github.com/repos/mmoyles87/kde-dynamic-wallpaper/releases/latest | "
                      "grep -o '\"browser_download_url\": \"[^\"]*\"' | "
                      "grep '\\.tar\\.xz' | "
                      "head -n 1 | "
                      "cut -d'\"' -f4";

    FILE *pf = popen(cmd, "r");
    if (!pf) {
        printf("Error: Failed to spin up API communication tunnel.\n");
        return -1;
    }

    if (fgets(download_url, sizeof(download_url), pf)) {
        download_url[strcspn(download_url, "\n")] = 0;
    }
    pclose(pf);

    if (strlen(download_url) == 0 || !strstr(download_url, "https://")) {
        printf("Error: Could not locate the latest .tar.xz package asset from the GitHub API release payload.\n");
        return -1;
    }

    printf("Found latest package URL: %s\n", download_url);
    printf("Downloading package data archive stream...\n");

    char download_cmd[MAX_PATH * 2];
    sprintf(download_cmd, "curl -L  -o %s \"%s\"", target_archive, download_url);
    if (system(download_cmd) != 0) {
        printf("Error: Download transfer operation dropped or interrupted.\n");
        return -1;
    }

    printf("Download successfully verified: %s\n", target_archive);
    
    printf("Installing plugin layouts into system profiles via kpackagetool6...\n");
    char install_cmd[MAX_PATH * 2];
    sprintf(install_cmd, "kpackagetool6 --type Plasma/Wallpaper --install %s", target_archive);
    
    int ret = system(install_cmd);
    unlink(target_archive); 
    
    return ret;
}

// Extract a single string value from raw JSON block
void get_json_string(const char *json, const char *key, char *output) {
    char search[128]; 
    sprintf(search, "\"%s\"", key);
    char *ptr = strstr(json, search); 
    if (!ptr) return;
    
    ptr = strchr(ptr, ':'); 
    if (!ptr) return;
    ptr = strchr(ptr, '"'); 
    if (!ptr) return;
    ptr++; 
    
    int i = 0;
    while (*ptr != '"' && i < MAX_PATH - 1) {
        output[i++] = *ptr++;
    }
    output[i] = '\0';
}

// Tokenizes integer arrays from arbitrary JSON payload paths
int get_safe_image(const char *json_content, const char *key, const char *position) {
    char search_key[128]; 
    sprintf(search_key, "\"%s\"", key);
    char *list_start = strstr(json_content, search_key); 
    if (!list_start) return 1;
    
    list_start = strchr(list_start, '['); 
    if (!list_start) return 1;
    list_start++; 
    
    int items[64], count = 0;
    while (*list_start != ']' && count < 64) {
        while (*list_start == ' ' || *list_start == ',') list_start++;
        if (*list_start == ']') break;
        if (sscanf(list_start, "%d", &items[count]) == 1) count++;
        while (*list_start >= '0' && *list_start <= '9') list_start++;
    }
    
    if (count == 0) return 1;
    if (strcmp(position, "start") == 0) return items[0];
    if (strcmp(position, "mid") == 0) return items[count / 2];
    if (strcmp(position, "end") == 0) return items[count - 1];
    return items[0];
}

// Handles binary stream copies cleanly
int copy_file(const char *src, const char *dst) {
    FILE *source = fopen(src, "rb"); 
    if (!source) return -1;
    
    FILE *dest = fopen(dst, "wb"); 
    if (!dest) { 
        fclose(source); 
        return -1; 
    }
    
    char buffer[BUFFER_SIZE]; 
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }
    
    fclose(source); 
    fclose(dest); 
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage Configurations:\n");
        printf("  Extract Theme:  %s <filename.ddw>\n", argv[0]);
        printf("  Install Engine: %s --install-plugin\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--install-plugin") == 0) {
        if (download_latest_plugin() == 0) {
            printf("Plugin installation completed successfully!\n");
        } else {
            printf("Plugin installation finished or was already deployed.\n");
        }
        return 0;
    }

    char *input_file = argv[1];
    if (!strstr(input_file, ".ddw") && !strstr(input_file, ".zip")) {
        printf("Error: Target file must be a .ddw or .zip package archive.\n");
        return 1;
    }

    // Get USER HOME environment variable dynamically
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        printf("Error: Could not resolve user $HOME environment path variable.\n");
        return 1;
    }

    // Create safe root directory paths inside user space
    char base_save_path[MAX_PATH];
    sprintf(base_save_path, "%s/Pictures/DynamicWallpapers", home_dir);
    mkdir(base_save_path, 0755); // Ensure folder exists

    // Strip theme name out of archive filename
    char theme_name[MAX_PATH];
    // Find last slash if full path is passed
    char *base_filename = strrchr(input_file, '/');
    if (!base_filename) base_filename = input_file;
    else base_filename++; // Move past the slash

    strcpy(theme_name, base_filename);
    char *ext = strstr(theme_name, ".ddw");
    if (!ext) ext = strstr(theme_name, ".zip");
    if (ext) *ext = '\0';

    // Construct the final, permanent target output directory path
    char output_dir[MAX_PATH];
    sprintf(output_dir, "%s/%s", base_save_path, theme_name);

    char tmp_dir[] = "./.wdd_scratch_tmp";
    char sys_cmd[MAX_PATH * 2];

    printf("Verifying and extracting package archive: %s...\n", input_file);
    mkdir(tmp_dir, 0755);
    
    sprintf(sys_cmd, "unzip -q -o \"%s\" -d %s", input_file, tmp_dir);
    if (system(sys_cmd) != 0) {
        printf("Error: Failed to decompress package archive.\n");
        rmdir(tmp_dir); 
        return 1;
    }

    char json_find_cmd[MAX_PATH], json_filename[MAX_PATH] = {0};
    sprintf(json_find_cmd, "find %s -maxdepth 1 -name \"*.json\" | head -n 1", tmp_dir);
    FILE *pf = popen(json_find_cmd, "r");
    if (pf) {
        if (fgets(json_filename, sizeof(json_filename), pf)) {
            json_filename[strcspn(json_filename, "\n")] = 0;
        }
        pclose(pf);
    }

    if (strlen(json_filename) == 0) {
        printf("Error: Missing internal JSON layout manifest.\n"); 
        return 1;
    }

    FILE *f = fopen(json_filename, "r");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
    char *json_content = malloc(fsize + 1); fread(json_content, 1, fsize, f); fclose(f);
    json_content[fsize] = '\0';

    char image_wildcard_pattern[MAX_PATH] = {0};
    get_json_string(json_content, "imageFilename", image_wildcard_pattern);

    int dawn_idx    = get_safe_image(json_content, "sunriseImageList", "start");
    int morning_idx = get_safe_image(json_content, "sunriseImageList", "end");
    int day_idx     = get_safe_image(json_content, "dayImageList", "mid");
    int evening_idx = get_safe_image(json_content, "sunsetImageList", "start");
    int dusk_idx    = get_safe_image(json_content, "sunsetImageList", "end");
    int night_idx   = get_safe_image(json_content, "nightImageList", "mid");

    struct { const char *slot_name; int img_index; } mappings[] = {
        {"1_dawn.jpg", dawn_idx}, {"2_early_morning.jpg", morning_idx}, {"3_day.jpg", day_idx},
        {"4_evening.jpg", evening_idx}, {"5_dusk.jpg", dusk_idx}, {"6_night.jpg", night_idx}
    };

    mkdir(output_dir, 0755);
    printf("Safely storing assets into protected user directory: %s/\n", output_dir);

    for (int i = 0; i < 6; i++) {
        char src_file[MAX_PATH], dst_file[MAX_PATH], frame_idx_str[16], resolved_filename[MAX_PATH];
        sprintf(frame_idx_str, "%d", mappings[i].img_index);
        char *asterisk_ptr = strchr(image_wildcard_pattern, '*');
        if (asterisk_ptr) {
            int prefix_len = asterisk_ptr - image_wildcard_pattern;
            strncpy(resolved_filename, image_wildcard_pattern, prefix_len); resolved_filename[prefix_len] = '\0';
            strcat(resolved_filename, frame_idx_str); strcat(resolved_filename, asterisk_ptr + 1);
        } else {
            strcpy(resolved_filename, image_wildcard_pattern);
        }
        sprintf(src_file, "%s/%s", tmp_dir, resolved_filename);
        sprintf(dst_file, "%s/%s", output_dir, mappings[i].slot_name);
        copy_file(src_file, dst_file);
    }

    sprintf(sys_cmd, "rm -rf %s", tmp_dir); system(sys_cmd);
    printf("Success! Target safe location: %s/\n", output_dir);
    free(json_content);
    return 0;
}

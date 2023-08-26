# Copyright 2023 Sascha Willems
# SPDX-License-Identifier: Apache-2.0

# Convert Vulkan Samples documentation into an Antora project

import shutil
import os

# Copy files into the structure expected by an Antora module

SAMPLES_ROOT = "../samples"
ANTORA_ROOT = "modules/ROOT"

def convert_folder(folder_name):
    dst_folder = ANTORA_ROOT + "/pages/" + folder_name
    dst_image_folder = ANTORA_ROOT + "/images/" + folder_name
    src_folder = SAMPLES_ROOT + "/" + folder_name
    os.makedirs(dst_folder, 0o777, True)
    for root, dirs, files in os.walk(src_folder):
        for file in files:
            # Docs
            if (file.endswith(".adoc")):                
                dst_sub_folder = dst_folder + "/" + os.path.basename(root)
                if os.path.basename(root) == folder_name:
                    shutil.copy(os.path.join(root, file), dst_folder + "/" + file)
                else:
                    os.makedirs(dst_sub_folder, 0o777, True)
                    shutil.copy(os.path.join(root, file), dst_sub_folder + "/" + file)
                    # We want/need to add in some additional stuff into the readme during the build process
                    with open(dst_sub_folder + "/" + file, "r+") as f:
                        s = f.read()
                        # We need to add the image folder to the doc to match how Antora expects images to be stored
                        added_text = ":images: " + root.replace("../samples/", "") + "\n"
                        # Add reference to sample in github repo so people can easily find the sample code
                        repo_node_added = False
                        repo_sample_url = "https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/" + root.replace("../samples/", "")
                        repo_note = "\n\nTIP: The source for this sample can be found in the " + repo_sample_url + "[Khronos Vulkan samples github repository].\n" 
                        first_caption_pos = s.find("=")
                        if first_caption_pos > -1:
                            end_line_pos = s.find("\n", first_caption_pos)
                            if end_line_pos > -1: 
                                s = s[:end_line_pos] + repo_note + s[end_line_pos:]
                                repo_node_added = True
                        if not repo_node_added:
                            print('Warning: Could not add repo link in ' + file)
                        f.seek(0)
                        f.write(added_text + s)
                        f.close()
            # Antora requires images to be in a predefined folder, which clashes with how images are stored in the repo (besides the docs)                    
            if file.endswith(".jpg") or file.endswith(".jpeg") or file.endswith(".png"):                
                dst_image_sub_folder = ANTORA_ROOT + "/images/" + root.replace("../samples/", "")
                os.makedirs(dst_image_sub_folder, 0o777, True)
                shutil.copy(os.path.join(root, file), dst_image_sub_folder + "/" + file)
 
convert_folder("api")
convert_folder("extensions")
convert_folder("performance")
convert_folder("tooling")
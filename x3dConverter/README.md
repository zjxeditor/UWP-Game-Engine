Module "LoadStaticModel" only extracts static mesh information from fbx files including: position, normal, tangent, UV, material, texture as well as triangle indices. Please note that some data rearrange work has been processed to reduce the final draw calls. Take a fbx file as the input, it will produce two .x3d files: one is in ASCII mode for easy verification and the other one is in binary mode for the mini engine. You can open ASCII files to see the .x3d file format. Module "LoadStaticModel_old" is less efficient. In order to load meshes which contain skinned animation, please use the "LoadDynamicModel" module.  

Requirement:  
Latest FBX SDK for windows desktop. Version 2016.1 is preferred.  

Note:  
1.x3d file format is based on the m3d file format which is invented by Frank D. Luna. Please refer to the book <<Introduction to 3D Game Programming with Direct11>>. 
2.Some sample x3d mesh data is provide in MetroGame/Media/Meshes/. Mesh's name will start with 'D' if it contains skinned animation.
target_sources(SRB2SDL2 PRIVATE
	i_main.c
	i_net.c
	i_sound.c
	i_system.c
	i_threads.c
	i_video.c
)
target_compile_definitions(SRB2SDL2 PRIVATE -DDEDICATED)

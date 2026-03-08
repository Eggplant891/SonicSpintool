#include <iostream>
#include <fstream>
#include <cassert>
#include <filesystem>
#include <vector>
#include <ostream>
#include <algorithm>

#define SDL_ENABLE_OLD_NAMES
#include "SDL3/SDL.h"
#include "backends/imgui_impl_sdl3.h"

#include "render.h"
#include "rom/spinball_rom.h"
#include "ui/ui_editor.h"

namespace spintool
{
	int SSEMain()
	{
		bool quitting = false;
		SDL_Init(SDL_INIT_VIDEO);
		SDL_Event event;

		Renderer::Initialise();

		EditorUI editor_ui;
		editor_ui.Initialise();
		while (quitting == false)
		{
			Renderer::NewFrame();
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL3_ProcessEvent(&event);
				if (event.type == SDL_QUIT)
				{
					quitting = true;
					break;
				}
			}

			editor_ui.Update();
			//Renderer::s_sdl_update_mutex.lock();
			Renderer::Render();
			//Renderer::s_sdl_update_mutex.unlock();
			SDL_Delay(16);
		}
		Renderer::Shutdown();
		SDL_Quit();

		return 0;
	}
}

int main(int argc, char* args[])
{
	return spintool::SSEMain();
}

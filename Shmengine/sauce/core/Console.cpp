#include "Console.hpp"
#include "Memory.hpp"
#include "Assert.hpp"

#include "utility/String.hpp"
#include "containers/Sarray.hpp"

namespace Console
{

	struct Consumer
	{
		FP_consumer_write callback;
		void* instance;
	};

	struct Command
	{
		const static uint32 max_command_length = 32;
		char name[max_command_length];
		uint32 arg_count;
		FP_command callback;
	};

	struct SystemState
	{
		const static uint32 max_consumer_count = 10;

		Consumer consumers[max_consumer_count];
		Darray<Command> registered_commands;
	};

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->registered_commands.init(8, 0);

		for (uint32 i = 0; i < SystemState::max_consumer_count; i++)
		{
			system_state->consumers[i].instance = 0;
			system_state->consumers[i].callback = 0;
		}

		return true;
	}

	void system_shutdown(void* state)
	{
		system_state->registered_commands.free_data();
	}

	bool32 register_consumer(void* inst, FP_consumer_write callback, uint32* out_consumer_id)
	{

		uint32 first_free_index = INVALID_ID;
		for (uint32 i = 0; i < SystemState::max_consumer_count; i++)
		{
			if (!system_state->consumers[i].instance && first_free_index == INVALID_ID)
			{
				first_free_index = i;
			}			
			else if (system_state->consumers[i].instance == inst)
			{
				system_state->consumers[i].callback = callback;
				*out_consumer_id = i;
				return true;
			}
		}

		*out_consumer_id = first_free_index;
		if (*out_consumer_id == INVALID_ID)
		{
			SHMERROR("Could not find free slot for console consumer.");
			return false;
		}

		Consumer* consumer = &system_state->consumers[first_free_index];
		consumer->instance = inst;
		consumer->callback = callback;
		return true;

	}

	void unregister_consumer(uint32 consumer_id)
	{
		SHMASSERT_MSG(consumer_id < system_state->max_consumer_count, "Invalid consumer id!");

		Consumer* consumer = &system_state->consumers[consumer_id];
		consumer->instance = 0;
		consumer->callback = 0;
	}

	void update_consumer(uint32 consumer_id, void* inst, FP_consumer_write callback)
	{
		SHMASSERT_MSG(consumer_id < system_state->max_consumer_count, "Invalid consumer id!");

		Consumer* consumer = &system_state->consumers[consumer_id];
		consumer->instance = inst;
		consumer->callback = callback;
	}

	void write_line(Log::LogLevel level, const char* message)
	{
		if (!system_state)
			return;

		for (uint32 i = 0; i < SystemState::max_consumer_count; i++)
		{
			if (system_state->consumers[i].instance)
				system_state->consumers[i].callback(system_state->consumers[i].instance, level, message);
		}			
	}

	bool32 register_command(const char* command, uint32 arg_count, FP_command callback)
	{

		for (uint32 i = 0; i < system_state->registered_commands.count; i++)
		{
			if (CString::equal_i(command, system_state->registered_commands[i].name))
				return true;
		}

		Command* cmd = system_state->registered_commands.emplace();
		cmd->arg_count = arg_count;
		cmd->callback = callback;
		CString::copy(Command::max_command_length, cmd->name, command);

		return true;

	}

	bool32 unregister_command(const char* command)
	{
		for (uint32 i = 0; i < system_state->registered_commands.count; i++)
		{
			if (CString::equal_i(command, system_state->registered_commands[i].name))
			{
				system_state->registered_commands.remove_at(i);
				return true;
			}
		}

		return false;
	}

	bool32 execute_command(const char* command)
	{

		uint32 parts_count = 1;
		const char* c = command;
		while (*c)
		{
			if (*c == ' ')
				parts_count++;

			c++;
		}

		Darray<String> parts(parts_count, 0);
		CString::split(command, parts, ' ');
		if (parts.count < 1)
		{
			parts.free_data();
			return false;
		}

		char echo[512] = {};
		CString::print_s(echo, 512, "-->%s", command);

		bool32 success = true;
		bool32 command_found = false;

		for (uint32 i = 0; i < system_state->registered_commands.count; i++)
		{
			Command* cmd = &system_state->registered_commands[i];
			if (CString::equal_i(cmd->name, parts[0].c_str()))
			{
				command_found = true;
				uint32 arg_count = parts.count - 1;

				if (system_state->registered_commands[i].arg_count != arg_count)
				{
					SHMERRORV("The console command '%s' requires %u arguments but %u were provided.", cmd->name, cmd->arg_count, arg_count);
					success = false;
				}
				else
				{
					CommandContext context = {};
					context.argument_count = cmd->arg_count;
					if (context.argument_count > 0) {
						Sarray<CommandArg> args(context.argument_count, 0);
						context.arguments = args.data;
						for (uint32 j = 0; j < cmd->arg_count; ++j)
							context.arguments[j].value = parts[j + 1].c_str();

					}
					cmd->callback(context);

				}
				break;
			}
		}

		if (!command_found)
		{
			SHMERRORV("The command '%s' does not exist.", parts[0].c_str());
			success = false;
		}

		parts.free_data();

		return success;

	}

}
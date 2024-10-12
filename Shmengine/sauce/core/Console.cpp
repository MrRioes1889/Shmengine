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
		uint32 consumer_count;
		Consumer consumers[max_consumer_count];

		Darray<Command> registered_commands;
	};

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate_callback allocator_callback, void*& out_state)
	{
		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		system_state->registered_commands.init(8, 0);

		return true;
	}

	void system_shutdown()
	{
		system_state->registered_commands.free_data();
	}

	void register_consumer(void* inst, FP_consumer_write callback)
	{
		SHMASSERT_MSG(system_state->consumer_count + 1 <= SystemState::max_consumer_count, "Exceeded max console consumers limit!");

		Consumer* consumer = &system_state->consumers[system_state->consumer_count++];
		consumer->instance = inst;
		consumer->callback = callback;
	}

	void write_line(Log::LogLevel level, const char* message)
	{
		for (uint32 i = 0; i < system_state->consumer_count; i++)
			system_state->consumers[i].callback(system_state->consumers[i].instance, level, message);
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

	bool32 execute_command(const char* command)
	{

		uint32 parts_count = 1;
		const char* c = command;
		while (c)
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
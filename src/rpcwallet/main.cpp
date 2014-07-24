#include "common/command_line.h"
#include "common/rpc_client.h"
#include "common/scoped_message_writer.h"
#include "common/util.h"
#include "rpc/core_rpc_server.h"
#include "daemonizer/daemonizer.h"
#include "misc_log_ex.h"
#include "rpcwallet/wallet_executor.h"
#include "rpcwallet/wallet_rpc_server_commands_defs.h"
#include "string_tools.h"
#include "version.h"

#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

namespace
{
  const command_line::arg_descriptor<std::string>   arg_log_file         = {"log-file", "Specify log file", "rpcwallet.log"};
  const command_line::arg_descriptor<uint32_t>      arg_log_level        = {"log-level", "", 0, true};
  const command_line::arg_descriptor<bool>          arg_stop             = {"stop", "Stop running daemon"};
}  // file-local namespace

int main(int argc, char const * argv[])
{
  try
  {
    epee::string_tools::set_module_name_and_folder(argv[0]);

    po::options_description hidden_options("Hidden options");

    po::options_description general_options("General options");
    command_line::add_arg(general_options, command_line::arg_help);
    command_line::add_arg(general_options, command_line::arg_version);
    command_line::add_arg(general_options, arg_stop);

    po::options_description wallet_options("Wallet options");
    command_line::add_arg(wallet_options, arg_log_file);
    command_line::add_arg(wallet_options, arg_log_level);

    tools::t_wallet_executor::init_options(hidden_options, general_options, wallet_options);

    po::positional_options_description positional_options;

    daemonizer::init_options(hidden_options, general_options);

    po::options_description visible_options;
    visible_options.add(general_options).add(wallet_options);

    po::options_description all_options;
    all_options.add(visible_options).add(hidden_options);

    po::variables_map vm;

    bool r = command_line::handle_error_helper(visible_options, [&]()
    {
      auto parser = po::command_line_parser(argc, argv).options(all_options).positional(positional_options);
      po::store(parser.run(), vm);
      po::notify(vm);

      return true;
    });
    if (!r)
      return 1;

    if (command_line::get_arg(vm, command_line::arg_help))
    {
      std::cout << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG << std::endl << std::endl;
      std::cout << "Usage: rpcwallet --wallet-file=<file> --rpc-bind-port=<port> [--daemon-address=<host>:<port>] [--rpc-bind-address=127.0.0.1]" << std::endl;
      std::cout << visible_options << std::endl;
      return 0;
    }
    else if (command_line::get_arg(vm, command_line::arg_version))
    {
      std::cout << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG << std::endl;
      return 0;
    }

    boost::filesystem::path relative_path_base = daemonizer::get_relative_path_base(vm);

    //set up logging options
    epee::log_space::get_set_log_detalisation_level(true, LOG_LEVEL_2);
    {
      boost::filesystem::path log_file_path{boost::filesystem::absolute(command_line::get_arg(vm, arg_log_file), relative_path_base)};

      epee::log_space::log_singletone::add_logger(
          LOGGER_FILE
        , log_file_path.filename().string().c_str()
        , log_file_path.parent_path().string().c_str()
        , LOG_LEVEL_4
        );
    }

    if(command_line::has_arg(vm, arg_log_level))
    {
      LOG_PRINT_L0("Setting log level = " << command_line::get_arg(vm, arg_log_level));
      epee::log_space::get_set_log_detalisation_level(true, command_line::get_arg(vm, arg_log_level));
    }

    if (command_line::arg_present(vm, arg_stop))
    {
      auto rpc_ip_str = command_line::get_arg(vm, cryptonote::core_rpc_server::arg_rpc_bind_ip);
      auto rpc_port_str = command_line::get_arg(vm, cryptonote::core_rpc_server::arg_rpc_bind_port);

      uint32_t rpc_ip;
      uint16_t rpc_port;
      if (!epee::string_tools::get_ip_int32_from_string(rpc_ip, rpc_ip_str))
      {
        std::cerr << "Invalid IP: " << rpc_ip_str << std::endl;
        return 1;
      }
      if (!epee::string_tools::get_xtype_from_string(rpc_port, rpc_port_str))
      {
        std::cerr << "Invalid port: " << rpc_port_str << std::endl;
        return 1;
      }

      tools::wallet_rpc::COMMAND_RPC_STOP::request req;
      tools::wallet_rpc::COMMAND_RPC_STOP::response res;

      tools::t_rpc_client rpc_client{rpc_ip, rpc_port};

      if (rpc_client.basic_json_rpc_request(req, res, "stop"))
      {
        tools::success_msg_writer() << "Stop message sent";
        return 0;
      }
      else
      {
        return 1;
      }
    }

    return daemonizer::daemonize(argc, argv, tools::t_wallet_executor{vm}, vm);
  }
  catch (std::exception const & ex)
  {
    LOG_ERROR("Exception in main! " << ex.what());
  }
  catch (...)
  {
    LOG_ERROR("Exception in main!");
  }
  return 1;
}
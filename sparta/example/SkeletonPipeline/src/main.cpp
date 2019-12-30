// <main.cpp> -*- C++ -*-


#include <iostream>

#include "ExampleSimulation.hpp" // Core model example simulator

#include "sparta/parsers/ConfigEmitterYAML.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/app/MultiDetailOptions.hpp"
#include "sparta/sparta.hpp"

// User-friendly usage that correspond with
// sparta::app::CommandLineSimulator options
const char USAGE[] =
    "Usage:\n"
    "    [--num-producers]"     // The number of producers in the example
    "    [-v]\n"                 // another example...
    "    [-h] <data file1> <data file2> ...\n"
    "\n";

constexpr char VERSION_VARNAME[] = "version"; // Name of option to show version
constexpr char DATA_FILE_VARNAME[] = "data-file"; // Name of data file given at the end of command line
constexpr char DATA_FILE_OPTIONS[] = "data-file"; // Data file options, which are flag independent

int main(int argc, char **argv)
{
    uint64_t num_producers = 1;
    std::vector<std::string> datafiles;

    // try/catch block to ensure proper destruction of the cls/sim classes in
    // the event of an error
    try{
        // Helper class for parsing command line arguments, setting up
        // the simulator, and running the simulator. All of the things
        // done by this classs can be done manually if desired. Use
        // the source for the CommandLineSimulator class as a starting
        // point
        sparta::app::CommandLineSimulator cls(USAGE);
        auto & app_opts = cls.getApplicationOptions();
        app_opts.add_options()
            (VERSION_VARNAME,
             "produce version message",
             "produce version message") // Brief
            ("num-producers",
             sparta::app::named_value<uint64_t>("PCOUNT", &num_producers)->default_value(num_producers),
             "For this example skeleton, this is just a count, or number, of producer objects to create")
            ("verbose,v", "Be noisy.", "Be very, very noisy")
            (DATA_FILE_OPTIONS,
             sparta::app::named_value<std::vector<std::string>>("DATAFILES", &datafiles),
             "Specifies the data files to look at")
            ;

        // Add any positional command-line options -- example
        po::positional_options_description& pos_opts = cls.getPositionalOptions();
        pos_opts.add(DATA_FILE_VARNAME, -1); // example, look for the <data file> at the end

        // Parse command line options and configure simulator
        int err_code = 0;
        if(!cls.parse(argc, argv, err_code)){
            return err_code; // Any errors already printed to cerr
        }

        for(auto & i : datafiles) {
            std::cout << "Got this data file: " << i << std::endl;
        }

        bool be_noisy = false;
        auto& vm = cls.getVariablesMap();
        if(vm.count("verbose")){
            be_noisy = true;
        }

        // Create the simulator object for population -- does not
        // instantiate nor run it.
        sparta::Scheduler scheduler;
        ExampleSimulator sim(scheduler, num_producers, be_noisy);

        cls.populateSimulation(&sim);
        cls.runSimulator(&sim);
        cls.postProcess(&sim);

    }catch(...){
        // Could still handle or log the exception here
        throw;
    }

    return 0;
}

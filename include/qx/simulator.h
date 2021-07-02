/**
 * @file    qx_simulator.h
 * @author	Nader Khammassi
 *          Imran Ashraf
 * @date	   23-12-16
 * @brief	qx simulator interface
 */

#ifndef QX_SIMULATOR_H
#define QX_SIMULATOR_H

#include "qx/core/circuit.h"
#include "qx/representation.h"
#include "qx/libqasm_interface.h"
#include "qx/version.h"
#include <qasm_semantic.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <map>
#include <stdint.h>

#include "qx/core/logger.h"

namespace qx
{

/**
 * simulator
 */
class simulator
{
protected:
    qx::qu_register * reg;
    compiler::QasmRepresentation ast;
    std::string file_path;

public:
    simulator() : reg(nullptr) { /*xpu::init();*/ }
    ~simulator() { /*xpu::clean();*/ }

    void set(std::string fp)
    {
        file_path = fp;
    }

    void parse_file() // private
    {
        FILE * qasm_file = fopen(file_path.c_str(), "r");
        if (!qasm_file)
        {
            QX_EOUT("Could not open " << file_path );
        }
        // construct libqasm parser and safely parse input file
        compiler::QasmSemanticChecker * parser;
        try
        {
            parser = new compiler::QasmSemanticChecker(qasm_file);
            fclose (qasm_file);
            ast = parser->getQasmRepresentation();
        }
        catch (std::exception &e)
        {
            QX_EOUT("parsing file " << file_path);
            QX_EOUT(e.what());
        }
    }

    bool measurement_averaging_enabled = false;

    /**
     * execute qasm file
     */
    void execute(size_t navg)
    {
        // parsing used to be done when set(*) was called
        // instead it is now the first thing for execute()
        parse_file();
        // quantum state and circuits
        size_t                     qubits = ast.numQubits();
        std::vector<qx::circuit*>  circuits;
        std::vector<qx::circuit*>  noisy_circuits;
        std::vector<qx::circuit*>  perfect_circuits;

        // error model parameters
        size_t                     total_errors      = 0;
        double                     error_probability = 0;
        qx::error_model_t          error_model       = qx::__unknown_error_model__;

        // create the quantum state
        QX_DOUT("Creating quantum register of " << qubits << " qubits... ");
        try
        {
            reg = new qx::qu_register(qubits);
        }
        catch(std::bad_alloc& exception)
        {
            QX_EOUT("Not enough memory, aborting");
            // xpu::clean();
        }
        catch(std::exception& exception)
        {
            QX_EOUT("Unexpected exception (" << exception.what() << "), aborting");
            // xpu::clean();
        }

        // convert libqasm ast to qx internal representation
        std::vector<compiler::SubCircuit> subcircuits = ast.getSubCircuits().getAllSubCircuits();
        for(auto subcircuit : subcircuits)
        {
            try
            {
                perfect_circuits.push_back(load_cqasm_code(qubits, subcircuit));
            }
            catch (std::string type)
            {
                QX_EOUT("Encountered unsupported gate: ");
                // xpu::clean();
            }
        }

        QX_DOUT("Loaded " << perfect_circuits.size() << " circuits.");

        // check whether an error model is specified
        if (ast.getErrorModelType() == "depolarizing_channel")
        {
            error_probability = ast.getErrorModelParameters().at(0);
            error_model       = qx::__depolarizing_channel__;
        }

        // measurement averaging
        if (navg)
        {
            measurement_averaging_enabled = true;
            if (error_model == qx::__depolarizing_channel__)
            {
                qx::measure m;
                for (size_t s=0; s<navg; ++s)
                {
                    reg->reset();
                    for (size_t i=0; i<perfect_circuits.size(); i++)
                    {
                        if (perfect_circuits[i]->size() == 0)
                            continue;
                        size_t iterations = perfect_circuits[i]->get_iterations();
                        if (iterations > 1)
                        {
                            for (size_t it=0; it<iterations; ++it)
                            {
                                qx::noisy_dep_ch(perfect_circuits[i],error_probability,total_errors)->execute(*reg,false,true);
                            }
                        }
                        else
                            qx::noisy_dep_ch(perfect_circuits[i],error_probability,total_errors)->execute(*reg,false,true);
                    }
                    m.apply(*reg);
                }
            }
            else
            {
                qx::measure m;
                for (size_t s=0; s<navg; ++s)
                {
                    reg->reset();
                    for (size_t i=0; i<perfect_circuits.size(); i++)
                        perfect_circuits[i]->execute(*reg,false,true);
                    m.apply(*reg);
                }
            }

            QX_DOUT("Average measurement after " << navg << " shots:");
            reg->dump(true);
        }
        else
        {
            if (error_model == qx::__depolarizing_channel__)
            {
                // QX_DOUT("[+] generating noisy circuits (p=" << qxr.getErrorProbability() << ")...");
                for (size_t i=0; i<perfect_circuits.size(); i++)
                {
                    if (perfect_circuits[i]->size() == 0)
                        continue;
                    // QX_DOUT("[>] processing circuit '" << perfect_circuits[i]->id() << "'...");
                    size_t iterations = perfect_circuits[i]->get_iterations();
                    if (iterations > 1)
                    {
                        for (size_t it=0; it<iterations; ++it)
                            circuits.push_back(qx::noisy_dep_ch(perfect_circuits[i],error_probability,total_errors));
                    }
                    else
                    {
                        circuits.push_back(qx::noisy_dep_ch(perfect_circuits[i],error_probability,total_errors));
                    }
                }
                // QX_DOUT("[+] total errors injected in all circuits : " << total_errors);
            }
            else
                circuits = perfect_circuits; // qxr.circuits();

            for (size_t i=0; i<circuits.size(); i++)
            {
                circuits[i]->execute(*reg);
            }
        }
    }

    std::vector<double> get_average_measurement()
    {        
        if(measurement_averaging_enabled)
        {
            std::vector<double> result;
            for(int i; i < ast.numQubits(); i++)
            {
                result.push_back(reg->get_average_measurement(i));
            }
            return result;
        }
        QX_EOUT("Average measurement not available");        
    }

    bool move(size_t q)
    {
        return reg->get_measurement(q);
    }

    std::string get_state()
    {
        return reg->get_state();
    }
};
}

#endif // QX_SIMULATOR_H

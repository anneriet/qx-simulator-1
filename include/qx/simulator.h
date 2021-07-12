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
    std::vector<double> get_average_measurement(size_t reps)
    {   
        std::binomial_distribution<size_t> distribution;
        std::random_device rd;
        std::mt19937 gen(xpu::timer().current()*rd());
        uint64_t size = ast.numQubits(); 

        std::vector<double> vec(size, 0); // to store the results

        uint64_t n = (1UL << size);

        uint64_t range = (n >> 1);
        cvector_t& pstates = reg->get_data();
        static const uint64_t SIZE = 1000;
        if(reps > 0) // this way we avoid divide-by-zero issues and also provide a way to get the normalized state vector directly (because then the state does not get replaced by a randomized average)
        {        
            for (int i = 0; i < n; i++)
            {
                distribution = std::binomial_distribution<size_t>(reps, pstates[i].norm());
                pstates[i] = std::sqrt((1./reps)*distribution(gen)); 
            } 
        }
        for(size_t qubit = 0; qubit < size; qubit++)
        {
            for (int64_t batch = 0; batch <= (int64_t)range / SIZE; batch++) {
                vec[qubit] += p1_worker(batch*SIZE, std::min<uint64_t>((batch+1)*SIZE, range), qubit, &pstates);
            }
        }
        return vec;
    }

    std::vector<double> get_average_measurement()
    {        
        if(measurement_averaging_enabled)
        {
            std::vector<double> vec;
            for(int i; i < ast.numQubits(); i++)
            {
                vec.push_back(reg->get_average_measurement(i));
            }
            return vec;
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

    std::vector<std::complex<double>> get_state_vector()
    {
        std::vector<std::complex<double>>  vec;
        for (auto &state : reg->get_data())
        {
            vec.push_back(std::complex<double>(state.re, state.im));
        }
        return vec;
    }
};
}

#endif // QX_SIMULATOR_H

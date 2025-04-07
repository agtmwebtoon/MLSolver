//
//  Parameters.h
//  main
//
//  Created by 이민형 on 1/21/25.
//

#ifndef Parameters_h
#define Parameters_h

#include <deal.II/base/parameter_handler.h>
#include <string>

namespace MLSolver {
using namespace dealii;

namespace Parameters {

struct FESystem {
    unsigned int poly_degree;
    unsigned int quad_order;

    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

struct Geometry {
    unsigned int global_refinement;
    double scale;
    double p_p0;
    int cellnum;
    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

struct Materials {
    double nu;
    double mu;

    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

struct LinearSolver {
    std::string type_lin;
    double tol_lin;
    double max_iterations_lin;
    bool use_static_condensation;
    std::string preconditioner_type;
    double preconditioner_relaxation;

    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

struct NonlinearSolver {
    unsigned int max_iterations_NR;
    double tol_f;
    double tol_u;

    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

struct Time {
    double delta_t;
    double end_time;

    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

struct AllParameters : public FESystem,
                       public Geometry,
                       public Materials,
                       public LinearSolver,
                       public NonlinearSolver,
                       public Time {
    AllParameters(const std::string &input_file);
    static void declare_parameters(ParameterHandler &prm);
    void parse_parameters(ParameterHandler &prm);
};

} // namespace Parameters
} // namespace MLSolver

#endif /* Parameters_h */

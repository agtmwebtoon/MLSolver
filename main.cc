/*
 * Authors: Jean-Paul Pelteret, University of Cape Town,
 *          Andrew McBride, University of Erlangen-Nuremberg, 2010
 * Modifier: Minhyung Lee (KAIST, mhlee@kaist.ac.kr)
 */

#include "FEM.h"
#include "util/Parameters.h"

namespace MLSolver {
using namespace dealii;

class Time {
  public:
    Time(const double time_end, const double delta_t)
        : timestep(0), time_current(0.0), time_end(time_end), delta_t(delta_t) {
    }

    virtual ~Time() = default;

    double current() const { return time_current; }
    double end() const { return time_end; }
    double get_delta_t() const { return delta_t; }
    unsigned int get_timestep() const { return timestep; }
    void increment() {
        time_current += delta_t;
        ++timestep;
    }

  private:
    unsigned int timestep;
    double time_current;
    const double time_end;
    const double delta_t;
};

template <int dim> class Material_Compressible_Neo_Hook_Three_Field {
  public:
    Material_Compressible_Neo_Hook_Three_Field(const double mu, const double nu)
        : kappa((2.0 * mu * (1.0 + nu)) / (3.0 * (1.0 - 2.0 * nu))),
          c_1(mu / 2.0), det_F(1.0), p_tilde(0.0), J_tilde(1.0),
          b_bar(Physics::Elasticity::StandardTensors<dim>::I) {
        Assert(kappa > 0,
               ExcMessage("The parameters mu and nu need to be so that kappa "
                          "has a positive value."));
    }

    void update_material_data(const Tensor<2, dim> &F, const double p_tilde_in,
                              const double J_tilde_in) {
        det_F = determinant(F);
        Assert(det_F > 0,
               ExcMessage("The tensor F must have a positive determinant."));

        const Tensor<2, dim> F_bar = Physics::Elasticity::Kinematics::F_iso(F);
        b_bar = Physics::Elasticity::Kinematics::b(F_bar);
        p_tilde = p_tilde_in;
        J_tilde = J_tilde_in;
    }

    SymmetricTensor<2, dim> get_tau() { return get_tau_iso() + get_tau_vol(); }

    SymmetricTensor<4, dim> get_Jc() const {
        return get_Jc_vol() + get_Jc_iso();
    }

    double get_dPsi_vol_dJ() const {
        return (kappa / 2.0) * (J_tilde - 1.0 / J_tilde);
    }

    double get_d2Psi_vol_dJ2() const {
        return ((kappa / 2.0) * (1.0 + 1.0 / (J_tilde * J_tilde)));
    }

    double get_det_F() const { return det_F; }

    double get_p_tilde() const { return p_tilde; }

    double get_J_tilde() const { return J_tilde; }

  protected:
    const double kappa;
    const double c_1;

    double det_F;
    double p_tilde;
    double J_tilde;
    SymmetricTensor<2, dim> b_bar;

    SymmetricTensor<2, dim> get_tau_vol() const {
        return p_tilde * det_F * Physics::Elasticity::StandardTensors<dim>::I;
    }

    SymmetricTensor<2, dim> get_tau_iso() const {
        return Physics::Elasticity::StandardTensors<dim>::dev_P * get_tau_bar();
    }

    SymmetricTensor<2, dim> get_tau_bar() const { return 2.0 * c_1 * b_bar; }

    SymmetricTensor<4, dim> get_Jc_vol() const {
        return p_tilde * det_F *
               (Physics::Elasticity::StandardTensors<dim>::IxI -
                (2.0 * Physics::Elasticity::StandardTensors<dim>::S));
    }

    SymmetricTensor<4, dim> get_Jc_iso() const {
        const SymmetricTensor<2, dim> tau_bar = get_tau_bar();
        const SymmetricTensor<2, dim> tau_iso = get_tau_iso();
        const SymmetricTensor<4, dim> tau_iso_x_I = outer_product(
            tau_iso, Physics::Elasticity::StandardTensors<dim>::I);
        const SymmetricTensor<4, dim> I_x_tau_iso = outer_product(
            Physics::Elasticity::StandardTensors<dim>::I, tau_iso);
        const SymmetricTensor<4, dim> c_bar = get_c_bar();

        return (2.0 / dim) * trace(tau_bar) *
                   Physics::Elasticity::StandardTensors<dim>::dev_P -
               (2.0 / dim) * (tau_iso_x_I + I_x_tau_iso) +
               Physics::Elasticity::StandardTensors<dim>::dev_P * c_bar *
                   Physics::Elasticity::StandardTensors<dim>::dev_P;
    }

    SymmetricTensor<4, dim> get_c_bar() const {
        return SymmetricTensor<4, dim>();
    }
};

template <int dim> class PointHistory {
  public:
    PointHistory()
        : F_inv(Physics::Elasticity::StandardTensors<dim>::I),
          tau(SymmetricTensor<2, dim>()), d2Psi_vol_dJ2(0.0), dPsi_vol_dJ(0.0),
          Jc(SymmetricTensor<4, dim>()) {}

    virtual ~PointHistory() = default;

    void setup_lqp(const Parameters::AllParameters &parameters) {
        material =
            std::make_shared<Material_Compressible_Neo_Hook_Three_Field<dim>>(
                parameters.mu, parameters.nu);
        update_values(Tensor<2, dim>(), 0.0, 1.0);
    }

    void update_values(const Tensor<2, dim> &Grad_u_n, const double p_tilde,
                       const double J_tilde) {
        const Tensor<2, dim> F = Physics::Elasticity::Kinematics::F(Grad_u_n);
        material->update_material_data(F, p_tilde, J_tilde);

        F_inv = invert(F);
        tau = material->get_tau();
        Jc = material->get_Jc();
        dPsi_vol_dJ = material->get_dPsi_vol_dJ();
        d2Psi_vol_dJ2 = material->get_d2Psi_vol_dJ2();
    }

    double get_J_tilde() const { return material->get_J_tilde(); }

    double get_det_F() const { return material->get_det_F(); }

    const Tensor<2, dim> &get_F_inv() const { return F_inv; }

    double get_p_tilde() const { return material->get_p_tilde(); }

    const SymmetricTensor<2, dim> &get_tau() const { return tau; }

    double get_dPsi_vol_dJ() const { return dPsi_vol_dJ; }

    double get_d2Psi_vol_dJ2() const { return d2Psi_vol_dJ2; }

    const SymmetricTensor<4, dim> &get_Jc() const { return Jc; }

  private:
    std::shared_ptr<Material_Compressible_Neo_Hook_Three_Field<dim>> material;

    Tensor<2, dim> F_inv;

    SymmetricTensor<2, dim> tau;
    double d2Psi_vol_dJ2;
    double dPsi_vol_dJ;

    SymmetricTensor<4, dim> Jc;
};

template <int dim> class Solid {
  public:
    Solid(const std::string &input_file);

    void run();

  private:
    struct PerTaskData_ASM;
    struct ScratchData_ASM;

    struct PerTaskData_SC;
    struct ScratchData_SC;

    struct PerTaskData_UQPH;
    struct ScratchData_UQPH;

    void make_grid();

    void make_grid_with_custom_mesh();

    void make_grid_cooks();

    void cooks_membrane_grid(const unsigned int);

    void system_setup();

    void make_constraints(const unsigned int it_nr);

    void assemble_system();

    void assemble_system_one_cell(
        const typename DoFHandler<dim>::active_cell_iterator &cell,
        ScratchData_ASM &scratch, PerTaskData_ASM &data) const;

    void assemble_sc();

    void assemble_sc_one_cell(
        const typename DoFHandler<dim>::active_cell_iterator &cell,
        ScratchData_SC &scratch, PerTaskData_SC &data);

    void copy_local_to_global_sc(const PerTaskData_SC &data);

    void setup_qph();

    void update_qph_incremental(const BlockVector<double> &solution_delta);

    void update_qph_incremental_one_cell(
        const typename DoFHandler<dim>::active_cell_iterator &cell,
        ScratchData_UQPH &scratch, PerTaskData_UQPH &data);

    void copy_local_to_global_UQPH(const PerTaskData_UQPH & /*data*/) {}

    void solve_nonlinear_timestep(BlockVector<double> &solution_delta);

    std::pair<unsigned int, double>
    solve_linear_system(BlockVector<double> &newton_update);

    BlockVector<double>
    get_total_solution(const BlockVector<double> &solution_delta) const;

    void output_results() const;

    Parameters::AllParameters parameters;

    double vol_reference;

    Triangulation<dim> triangulation;

    Time time;
    mutable TimerOutput timer;

    CellDataStorage<typename Triangulation<dim>::cell_iterator,
                    PointHistory<dim>>
        quadrature_point_history;

    const unsigned int degree;
    const FESystem<dim> fe;
    DoFHandler<dim> dof_handler;
    const unsigned int dofs_per_cell;

    static constexpr unsigned int n_blocks = 3;
    static constexpr unsigned int n_components = dim + 2;
    static constexpr unsigned int first_u_component = 0;
    static constexpr unsigned int p_component = dim;
    static constexpr unsigned int J_component = dim + 1;

    FEValuesExtractors::Vector u_fe =
        FEValuesExtractors::Vector(first_u_component);
    FEValuesExtractors::Scalar p_fe = FEValuesExtractors::Scalar(p_component);
    FEValuesExtractors::Scalar J_fe = FEValuesExtractors::Scalar(J_component);

    enum { u_dof = 0, p_dof = 1, J_dof = 2 };

    std::vector<types::global_dof_index> dofs_per_block;
    std::vector<types::global_dof_index> element_indices_u;
    std::vector<types::global_dof_index> element_indices_p;
    std::vector<types::global_dof_index> element_indices_J;

    const QGauss<dim> qf_cell;
    const QGauss<dim - 1> qf_face;
    const unsigned int n_q_points;
    const unsigned int n_q_points_f;

    AffineConstraints<double> constraints;
    BlockSparsityPattern sparsity_pattern;
    BlockSparseMatrix<double> tangent_matrix;
    BlockVector<double> system_rhs;
    BlockVector<double> solution_n;

    struct Errors {
        Errors() : norm(1.0), u(1.0), p(1.0), J(1.0) {}

        void reset() {
            norm = 1.0;
            u = 1.0;
            p = 1.0;
            J = 1.0;
        }
        void normalize(const Errors &rhs) {
            if (rhs.norm != 0.0)
                norm /= rhs.norm;
            if (rhs.u != 0.0)
                u /= rhs.u;
            if (rhs.p != 0.0)
                p /= rhs.p;
            if (rhs.J != 0.0)
                J /= rhs.J;
        }

        double norm, u, p, J;
    };

    Errors error_residual, error_residual_0, error_residual_norm, error_update,
        error_update_0, error_update_norm;

    void get_error_residual(Errors &error_residual);

    void get_error_update(const BlockVector<double> &newton_update,
                          Errors &error_update);

    std::pair<double, double> get_error_dilation() const;

    double compute_vol_current() const;

    static void print_conv_header();

    void print_conv_footer();
};

template <int dim>
Solid<dim>::Solid(const std::string &input_file)
    : parameters(input_file), vol_reference(0.),
      triangulation(Triangulation<dim>::maximum_smoothing),
      time(parameters.end_time, parameters.delta_t),
      timer(std::cout, TimerOutput::summary, TimerOutput::wall_times),
      degree(parameters.poly_degree),
      fe(FE_Q<dim>(parameters.poly_degree) ^ dim, // displacement
         FE_DGP<dim>(parameters.poly_degree - 1), // pressure
         FE_DGP<dim>(parameters.poly_degree - 1)) // dilatation
      ,
      dof_handler(triangulation), dofs_per_cell(fe.n_dofs_per_cell()),
      dofs_per_block(n_blocks), qf_cell(parameters.quad_order),
      qf_face(parameters.quad_order), n_q_points(qf_cell.size()),
      n_q_points_f(qf_face.size()) {
    Assert(dim == 2 || dim == 3,
           ExcMessage("This problem only works in 2 or 3 space dimensions."));

    for (unsigned int k = 0; k < fe.n_dofs_per_cell(); ++k) {
        // 자유도가 속한 컴포넌트를 확인
        const unsigned int component = fe.system_to_component_index(k).first;

        if (component >= first_u_component && component < p_component) // 변위
            element_indices_u.push_back(k);
        else if (component == p_component) // 압력
            element_indices_p.push_back(k);
        else if (component == J_component) // 팽창
            element_indices_J.push_back(k);
        else
            DEAL_II_ASSERT_UNREACHABLE();
    }
}

template <int dim> void Solid<dim>::run() {
    // make_grid_cooks();
    cooks_membrane_grid(parameters.cellnum);
    // make_grid();
    system_setup();
    {
        AffineConstraints<double> constraints;
        constraints.close();

        const ComponentSelectFunction<dim> J_mask(J_component, n_components);

        VectorTools::project(dof_handler, constraints, QGauss<dim>(degree + 2),
                             J_mask, solution_n);
    }
    output_results();
    time.increment();

    BlockVector<double> solution_delta(dofs_per_block);
    while (time.current() < time.end()) {
        solution_delta = 0.0;

        solve_nonlinear_timestep(solution_delta);
        solution_n += solution_delta;

        output_results();
        time.increment();
    }
}

template <int dim> struct Solid<dim>::PerTaskData_ASM {
    FullMatrix<double> cell_matrix;
    Vector<double> cell_rhs;
    std::vector<types::global_dof_index> local_dof_indices;

    PerTaskData_ASM(const unsigned int dofs_per_cell)
        : cell_matrix(dofs_per_cell, dofs_per_cell), cell_rhs(dofs_per_cell),
          local_dof_indices(dofs_per_cell) {}

    void reset() {
        cell_matrix = 0.0;
        cell_rhs = 0.0;
    }
};

template <int dim> struct Solid<dim>::ScratchData_ASM {
    FEValues<dim> fe_values;
    FEFaceValues<dim> fe_face_values;

    std::vector<std::vector<double>> Nx;
    std::vector<std::vector<Tensor<2, dim>>> grad_Nx;
    std::vector<std::vector<SymmetricTensor<2, dim>>> symm_grad_Nx;

    ScratchData_ASM(const FiniteElement<dim> &fe_cell,
                    const QGauss<dim> &qf_cell, const UpdateFlags uf_cell,
                    const QGauss<dim - 1> &qf_face, const UpdateFlags uf_face)
        : fe_values(fe_cell, qf_cell, uf_cell),
          fe_face_values(fe_cell, qf_face, uf_face),
          Nx(qf_cell.size(), std::vector<double>(fe_cell.n_dofs_per_cell())),
          grad_Nx(qf_cell.size(),
                  std::vector<Tensor<2, dim>>(fe_cell.n_dofs_per_cell())),
          symm_grad_Nx(qf_cell.size(), std::vector<SymmetricTensor<2, dim>>(
                                           fe_cell.n_dofs_per_cell())) {}

    ScratchData_ASM(const ScratchData_ASM &rhs)
        : fe_values(rhs.fe_values.get_fe(), rhs.fe_values.get_quadrature(),
                    rhs.fe_values.get_update_flags()),
          fe_face_values(rhs.fe_face_values.get_fe(),
                         rhs.fe_face_values.get_quadrature(),
                         rhs.fe_face_values.get_update_flags()),
          Nx(rhs.Nx), grad_Nx(rhs.grad_Nx), symm_grad_Nx(rhs.symm_grad_Nx) {}

    void reset() {
        const unsigned int n_q_points = Nx.size();
        const unsigned int n_dofs_per_cell = Nx[0].size();
        for (unsigned int q_point = 0; q_point < n_q_points; ++q_point) {
            AssertDimension(Nx[q_point].size(), n_dofs_per_cell);
            AssertDimension(grad_Nx[q_point].size(), n_dofs_per_cell);
            AssertDimension(symm_grad_Nx[q_point].size(), n_dofs_per_cell);

            for (unsigned int k = 0; k < n_dofs_per_cell; ++k) {
                Nx[q_point][k] = 0.0;
                grad_Nx[q_point][k] = 0.0;
                symm_grad_Nx[q_point][k] = 0.0;
            }
        }
    }
};

template <int dim> struct Solid<dim>::PerTaskData_SC {
    FullMatrix<double> cell_matrix;
    std::vector<types::global_dof_index> local_dof_indices;

    FullMatrix<double> k_orig;
    FullMatrix<double> k_pu;
    FullMatrix<double> k_pJ;
    FullMatrix<double> k_JJ;
    FullMatrix<double> k_pJ_inv;
    FullMatrix<double> k_bbar;
    FullMatrix<double> A;
    FullMatrix<double> B;
    FullMatrix<double> C;

    PerTaskData_SC(const unsigned int dofs_per_cell, const unsigned int n_u,
                   const unsigned int n_p, const unsigned int n_J)
        : cell_matrix(dofs_per_cell, dofs_per_cell),
          local_dof_indices(dofs_per_cell),
          k_orig(dofs_per_cell, dofs_per_cell), k_pu(n_p, n_u), k_pJ(n_p, n_J),
          k_JJ(n_J, n_J), k_pJ_inv(n_p, n_J), k_bbar(n_u, n_u), A(n_J, n_u),
          B(n_J, n_u), C(n_p, n_u) {}

    void reset() {}
};

template <int dim> struct Solid<dim>::ScratchData_SC {
    void reset() {}
};

template <int dim> struct Solid<dim>::PerTaskData_UQPH {
    void reset() {}
};

template <int dim> struct Solid<dim>::ScratchData_UQPH {
    const BlockVector<double> &solution_total;

    std::vector<Tensor<2, dim>> solution_grads_u_total;
    std::vector<double> solution_values_p_total;
    std::vector<double> solution_values_J_total;

    FEValues<dim> fe_values;

    ScratchData_UQPH(const FiniteElement<dim> &fe_cell,
                     const QGauss<dim> &qf_cell, const UpdateFlags uf_cell,
                     const BlockVector<double> &solution_total)
        : solution_total(solution_total),
          solution_grads_u_total(qf_cell.size()),
          solution_values_p_total(qf_cell.size()),
          solution_values_J_total(qf_cell.size()),
          fe_values(fe_cell, qf_cell, uf_cell) {}

    ScratchData_UQPH(const ScratchData_UQPH &rhs)
        : solution_total(rhs.solution_total),
          solution_grads_u_total(rhs.solution_grads_u_total),
          solution_values_p_total(rhs.solution_values_p_total),
          solution_values_J_total(rhs.solution_values_J_total),
          fe_values(rhs.fe_values.get_fe(), rhs.fe_values.get_quadrature(),
                    rhs.fe_values.get_update_flags()) {}

    void reset() {
        const unsigned int n_q_points = solution_grads_u_total.size();
        for (unsigned int q = 0; q < n_q_points; ++q) {
            solution_grads_u_total[q] = 0.0;
            solution_values_p_total[q] = 0.0;
            solution_values_J_total[q] = 0.0;
        }
    }
};

template <int dim> void Solid<dim>::make_grid() {
    GridGenerator::hyper_rectangle(
        triangulation,
        (dim == 3 ? Point<dim>(0.0, 0.0, 0.0) : Point<dim>(0.0, 0.0)),
        (dim == 3 ? Point<dim>(1.0, 1.0, 1.0) : Point<dim>(1.0, 1.0)), true);
    GridTools::scale(parameters.scale, triangulation);
    triangulation.refine_global(std::max(1U, parameters.global_refinement));

    vol_reference = GridTools::volume(triangulation);
    std::cout << "Grid:\n\t Reference volume: " << vol_reference << std::endl;
    int cnt = 0;
    for (const auto &cell : triangulation.active_cell_iterators())
        for (const auto &face : cell->face_iterators()) {
            if (face->at_boundary() == true &&
                face->center()[1] == 1.0 * parameters.scale) {
                if (dim == 3) {
                    if ((0.25 * parameters.scale < face->center()[0] &&
                         face->center()[0] < 0.75 * parameters.scale) &&
                        (0.25 * parameters.scale < face->center()[2] &&
                         face->center()[2] < 0.75 * parameters.scale))
                        face->set_boundary_id(6);
                } else {
                    if (face->center()[0] < 0.5 * parameters.scale)
                        face->set_boundary_id(6);
                }
            }
        }

    std::cout << cnt << std::endl;
}

template <int dim> Point<dim> grid_y_transform(const Point<dim> &pt_in) {
    const double &x = pt_in[0];
    const double &y = pt_in[1];

    const double y_upper = 44.0 + (16.0 / 48.0) * x; // top edge line
    const double y_lower = 0.0 + (44.0 / 48.0) * x;  // bottom edge line
    const double theta = y / 44.0;

    const double y_transform = (1 - theta) * y_lower + theta * y_upper;

    Point<dim> pt_out = pt_in;
    pt_out[1] = y_transform;

    return pt_out;
}

template <int dim>
void Solid<dim>::cooks_membrane_grid(const unsigned int elements_per_edge) {

    std::vector<unsigned int> repetitions(dim, elements_per_edge);

    if (dim == 3)
        repetitions[2] = 2; // thickness direction

    const Point<dim> bottom_left =
        (dim == 3 ? Point<dim>(0.0, 0.0, -2.5) : Point<dim>(0.0, 0.0));
    const Point<dim> top_right =
        (dim == 3 ? Point<dim>(48.0, 44.0, 2.5) : Point<dim>(48.0, 44.0));

    GridGenerator::subdivided_hyper_rectangle(triangulation, repetitions,
                                              bottom_left, top_right);

    // Assign boundary IDs
    const double tol = 1e-6;
    for (auto cell : triangulation.active_cell_iterators())
        for (unsigned int f = 0; f < GeometryInfo<dim>::faces_per_cell; ++f)
            if (cell->face(f)->at_boundary()) {
                const double x = cell->face(f)->center()[0];
                if (std::abs(x - 0.0) < tol)
                    cell->face(f)->set_boundary_id(1); // -X
                else if (std::abs(x - 48.0) < tol && std::abs(x - 48.0) < tol )
                    cell->face(f)->set_boundary_id(11); // +X
                else if (dim == 3 &&
                         std::abs(std::abs(cell->face(f)->center()[2]) - 0.5) <
                             tol)
                    cell->face(f)->set_boundary_id(2); // +Z / -Z
                else
                    cell->face(f)->set_boundary_id(3);
            }

    // Transform y-axis for Cook's beam shape
    GridTools::transform(&grid_y_transform<dim>, triangulation);

    GridTools::scale(parameters.scale, triangulation);
    vol_reference = GridTools::volume(triangulation);
    std::cout << "Grid:\n\t Reference volume: " << vol_reference << std::endl;

    std::cout << "Cook's membrane grid created with "
              << triangulation.n_active_cells() << " active cells."
              << std::endl;
}

template <int dim> void Solid<dim>::make_grid_with_custom_mesh() {
    GridIn<dim> grid_in;
    grid_in.attach_triangulation(triangulation);

    std::cout << "Current working directory: "
              << std::filesystem::current_path() << std::endl;

    const std::string filename = "output_sample.ucd";
    std::ifstream input_file(filename);

    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file '" << filename
                  << "'. Please check the file path." << std::endl;
        return;
    }

    grid_in.read_ucd(input_file);
    std::cout << "Successfully read UCD file: " << filename << std::endl;
    GridTools::scale(0.01, triangulation);

    vol_reference = GridTools::volume(triangulation);
    std::cout << "Grid:\n\t Reference volume: " << vol_reference << std::endl;

    // Find min and max x-coordinates
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();

    for (const auto &vertex : triangulation.get_vertices()) {
        x_min = std::min(x_min, vertex[0]);
        x_max = std::max(x_max, vertex[0]);
    }

    std::cout << "x_min: " << x_min << ", x_max: " << x_max << std::endl;
    int boundary_cnt_min = 0;
    int boundary_cnt_max = 0;
    //  Assign boundary IDs based on x value
    for (const auto &cell : triangulation.active_cell_iterators()) {
        for (unsigned int f = 0; f < cell->n_faces(); ++f) {
            if (cell->face(f)->at_boundary()) {
                const Point<dim> face_center = cell->face(f)->center();
                const double x_coord = face_center[0];
                // Assign boundary (x == -1) for fixed b.c.
                if (std::abs(x_coord + 0.38) < 1e-2) {
                    cell->face(f)->set_boundary_id(0);
                    boundary_cnt_min++;
                }

                // Assign boundary (x == 1) for Neumann b.c.
                else if (std::abs(x_coord - 0.38) < 1e-2) {
                    cell->face(f)->set_boundary_id(1);
                    boundary_cnt_max++;
                }

                else {
                    cell->face(f)->set_boundary_id(2);
                }
            }
        }
    }

    std::cout << boundary_cnt_min << std::endl;
    std::cout << boundary_cnt_max << std::endl;
}

template <int dim> void Solid<dim>::system_setup() {
    timer.enter_subsection("Setup system");

    std::vector<unsigned int> block_component(n_components,
                                              u_dof); // Displacement
    block_component[p_component] = p_dof;             // Pressure
    block_component[J_component] = J_dof;             // Dilatation

    std::cout << "Number of active cells: " << triangulation.n_active_cells()
              << std::endl;
    std::cout << "Number of vertices: " << triangulation.n_vertices()
              << std::endl;

    std::cout << "FE degree: " << fe.degree << std::endl;
    std::cout << "Number of DOFs per cell: " << fe.n_dofs_per_cell()
              << std::endl;

    dof_handler.distribute_dofs(fe);
    DoFRenumbering::Cuthill_McKee(dof_handler);
    DoFRenumbering::component_wise(dof_handler, block_component);

    dofs_per_block = dofs_per_block =
        DoFTools::count_dofs_per_fe_block(dof_handler, block_component);

    std::cout << "Triangulation:"
              << "\n\t Number of active cells: "
              << triangulation.n_active_cells()
              << "\n\t Number of degrees of freedom: " << dof_handler.n_dofs()
              << std::endl;

    tangent_matrix.clear();
    {
        BlockDynamicSparsityPattern dsp(dofs_per_block, dofs_per_block);

        Table<2, DoFTools::Coupling> coupling(n_components, n_components);
        for (unsigned int ii = 0; ii < n_components; ++ii)
            for (unsigned int jj = 0; jj < n_components; ++jj)
                if (((ii < p_component) && (jj == J_component)) ||
                    ((ii == J_component) && (jj < p_component)) ||
                    ((ii == p_component) && (jj == p_component)))
                    coupling[ii][jj] = DoFTools::none;
                else
                    coupling[ii][jj] = DoFTools::always;
        DoFTools::make_sparsity_pattern(dof_handler, coupling, dsp, constraints,
                                        false);
        sparsity_pattern.copy_from(dsp);
    }

    tangent_matrix.reinit(sparsity_pattern);

    system_rhs.reinit(dofs_per_block);
    solution_n.reinit(dofs_per_block);

    setup_qph();

    timer.leave_subsection();
}

template <int dim> void Solid<dim>::setup_qph() {
    std::cout << "    Setting up quadrature point data..." << std::endl;

    quadrature_point_history.initialize(triangulation.begin_active(),
                                        triangulation.end(), n_q_points);

    for (const auto &cell : triangulation.active_cell_iterators()) {
        const std::vector<std::shared_ptr<PointHistory<dim>>> lqph =
            quadrature_point_history.get_data(cell);
        AssertDimension(lqph.size(), n_q_points);

        for (unsigned int q_point = 0; q_point < n_q_points; ++q_point)
            lqph[q_point]->setup_lqp(parameters);
    }
}

template <int dim>
void Solid<dim>::update_qph_incremental(
    const BlockVector<double> &solution_delta) {
    timer.enter_subsection("Update QPH data");
    std::cout << " UQPH " << std::flush;

    const BlockVector<double> solution_total(
        get_total_solution(solution_delta));

    const UpdateFlags uf_UQPH(update_values | update_gradients);
    PerTaskData_UQPH per_task_data_UQPH;
    ScratchData_UQPH scratch_data_UQPH(fe, qf_cell, uf_UQPH, solution_total);

    WorkStream::run(dof_handler.active_cell_iterators(), *this,
                    &Solid::update_qph_incremental_one_cell,
                    &Solid::copy_local_to_global_UQPH, scratch_data_UQPH,
                    per_task_data_UQPH);

    timer.leave_subsection();
}

template <int dim>
void Solid<dim>::update_qph_incremental_one_cell(
    const typename DoFHandler<dim>::active_cell_iterator &cell,
    ScratchData_UQPH &scratch, PerTaskData_UQPH & /*data*/) {
    const std::vector<std::shared_ptr<PointHistory<dim>>> lqph =
        quadrature_point_history.get_data(cell);
    AssertDimension(lqph.size(), n_q_points);

    AssertDimension(scratch.solution_grads_u_total.size(), n_q_points);
    AssertDimension(scratch.solution_values_p_total.size(), n_q_points);
    AssertDimension(scratch.solution_values_J_total.size(), n_q_points);

    scratch.reset();

    scratch.fe_values.reinit(cell);
    scratch.fe_values[u_fe].get_function_gradients(
        scratch.solution_total, scratch.solution_grads_u_total);
    scratch.fe_values[p_fe].get_function_values(
        scratch.solution_total, scratch.solution_values_p_total);
    scratch.fe_values[J_fe].get_function_values(
        scratch.solution_total, scratch.solution_values_J_total);

    for (const unsigned int q_point :
         scratch.fe_values.quadrature_point_indices())
        lqph[q_point]->update_values(scratch.solution_grads_u_total[q_point],
                                     scratch.solution_values_p_total[q_point],
                                     scratch.solution_values_J_total[q_point]);
}

template <int dim>
void Solid<dim>::solve_nonlinear_timestep(BlockVector<double> &solution_delta) {
    std::cout << std::endl
              << "Timestep " << time.get_timestep() << " @ " << time.current()
              << 's' << std::endl;

    BlockVector<double> newton_update(dofs_per_block);

    error_residual.reset();
    error_residual_0.reset();
    error_residual_norm.reset();
    error_update.reset();
    error_update_0.reset();
    error_update_norm.reset();

    print_conv_header();

    unsigned int newton_iteration = 0;
    for (; newton_iteration < parameters.max_iterations_NR;
         ++newton_iteration) {
        std::cout << ' ' << std::setw(2) << newton_iteration << ' '
                  << std::flush;

        make_constraints(newton_iteration);
        assemble_system();

        get_error_residual(error_residual);
        if (newton_iteration == 0)
            error_residual_0 = error_residual;

        error_residual_norm = error_residual;
        error_residual_norm.normalize(error_residual_0);

        if (newton_iteration > 0 && error_update_norm.u <= parameters.tol_u &&
            error_residual_norm.u <= parameters.tol_f) {
            std::cout << " CONVERGED! " << std::endl;
            print_conv_footer();

            break;
        }

        const std::pair<unsigned int, double> lin_solver_output =
            solve_linear_system(newton_update);

        get_error_update(newton_update, error_update);
        if (newton_iteration == 0)
            error_update_0 = error_update;

        error_update_norm = error_update;
        error_update_norm.normalize(error_update_0);

        solution_delta += newton_update;
        update_qph_incremental(solution_delta);

        std::cout << " | " << std::fixed << std::setprecision(3) << std::setw(7)
                  << std::scientific << lin_solver_output.first << "  "
                  << lin_solver_output.second << "  "
                  << error_residual_norm.norm << "  " << error_residual_norm.u
                  << "  " << error_residual_norm.p << "  "
                  << error_residual_norm.J << "  " << error_update_norm.norm
                  << "  " << error_update_norm.u << "  " << error_update_norm.p
                  << "  " << error_update_norm.J << "  " << std::endl;
    }

    AssertThrow(newton_iteration < parameters.max_iterations_NR,
                ExcMessage("No convergence in nonlinear solver!"));
}

template <int dim> void Solid<dim>::print_conv_header() {
    static const unsigned int l_width = 150;

    for (unsigned int i = 0; i < l_width; ++i)
        std::cout << '_';
    std::cout << std::endl;

    std::cout << "               SOLVER STEP               "
              << " |  LIN_IT   LIN_RES    RES_NORM    "
              << " RES_U     RES_P      RES_J     NU_NORM     "
              << " NU_U       NU_P       NU_J " << std::endl;

    for (unsigned int i = 0; i < l_width; ++i)
        std::cout << '_';
    std::cout << std::endl;
}

template <int dim> void Solid<dim>::print_conv_footer() {
    static const unsigned int l_width = 150;

    for (unsigned int i = 0; i < l_width; ++i)
        std::cout << '_';
    std::cout << std::endl;

    const std::pair<double, double> error_dil = get_error_dilation();

    std::cout << "Relative errors:" << std::endl
              << "Displacement:\t" << error_update.u / error_update_0.u
              << std::endl
              << "Force: \t\t" << error_residual.u / error_residual_0.u
              << std::endl
              << "Dilatation:\t" << error_dil.first << std::endl
              << "v / V_0:\t" << error_dil.second * vol_reference << " / "
              << vol_reference << " = " << error_dil.second << std::endl;
}

template <int dim> double Solid<dim>::compute_vol_current() const {
    double vol_current = 0.0;

    FEValues<dim> fe_values(fe, qf_cell, update_JxW_values);

    for (const auto &cell : triangulation.active_cell_iterators()) {
        fe_values.reinit(cell);

        const std::vector<std::shared_ptr<const PointHistory<dim>>> lqph =
            quadrature_point_history.get_data(cell);
        AssertDimension(lqph.size(), n_q_points);

        for (const unsigned int q_point :
             fe_values.quadrature_point_indices()) {
            const double det_F_qp = lqph[q_point]->get_det_F();
            const double JxW = fe_values.JxW(q_point);

            vol_current += det_F_qp * JxW;
        }
    }
    Assert(vol_current > 0.0, ExcInternalError());
    return vol_current;
}

template <int dim>
std::pair<double, double> Solid<dim>::get_error_dilation() const {
    double dil_L2_error = 0.0;

    FEValues<dim> fe_values(fe, qf_cell, update_JxW_values);

    for (const auto &cell : triangulation.active_cell_iterators()) {
        fe_values.reinit(cell);

        const std::vector<std::shared_ptr<const PointHistory<dim>>> lqph =
            quadrature_point_history.get_data(cell);
        AssertDimension(lqph.size(), n_q_points);

        for (const unsigned int q_point :
             fe_values.quadrature_point_indices()) {
            const double det_F_qp = lqph[q_point]->get_det_F();
            const double J_tilde_qp = lqph[q_point]->get_J_tilde();
            const double the_error_qp_squared =
                Utilities::fixed_power<2>((det_F_qp - J_tilde_qp));
            const double JxW = fe_values.JxW(q_point);

            dil_L2_error += the_error_qp_squared * JxW;
        }
    }

    return std::make_pair(std::sqrt(dil_L2_error),
                          compute_vol_current() / vol_reference);
}

template <int dim> void Solid<dim>::get_error_residual(Errors &error_residual) {
    BlockVector<double> error_res(dofs_per_block);

    for (unsigned int i = 0; i < dof_handler.n_dofs(); ++i)
        if (!constraints.is_constrained(i))
            error_res(i) = system_rhs(i);

    error_residual.norm = error_res.l2_norm();
    error_residual.u = error_res.block(u_dof).l2_norm();
    error_residual.p = error_res.block(p_dof).l2_norm();
    error_residual.J = error_res.block(J_dof).l2_norm();
}

template <int dim>
void Solid<dim>::get_error_update(const BlockVector<double> &newton_update,
                                  Errors &error_update) {
    BlockVector<double> error_ud(dofs_per_block);
    for (unsigned int i = 0; i < dof_handler.n_dofs(); ++i)
        if (!constraints.is_constrained(i))
            error_ud(i) = newton_update(i);

    error_update.norm = error_ud.l2_norm();
    error_update.u = error_ud.block(u_dof).l2_norm();
    error_update.p = error_ud.block(p_dof).l2_norm();
    error_update.J = error_ud.block(J_dof).l2_norm();
}

template <int dim>
BlockVector<double> Solid<dim>::get_total_solution(
    const BlockVector<double> &solution_delta) const {
    BlockVector<double> solution_total(solution_n);
    solution_total += solution_delta;
    return solution_total;
}

template <int dim> void Solid<dim>::assemble_system() {
    timer.enter_subsection("Assemble system");
    std::cout << " ASM_SYS " << std::flush;

    tangent_matrix = 0.0;
    system_rhs = 0.0;

    const UpdateFlags uf_cell(update_values | update_gradients |
                              update_JxW_values);
    const UpdateFlags uf_face(update_values | update_normal_vectors |
                              update_JxW_values);

    PerTaskData_ASM per_task_data(dofs_per_cell);
    ScratchData_ASM scratch_data(fe, qf_cell, uf_cell, qf_face, uf_face);

    WorkStream::run(
        dof_handler.active_cell_iterators(),
        [this](const typename DoFHandler<dim>::active_cell_iterator &cell,
               ScratchData_ASM &scratch, PerTaskData_ASM &data) {
            this->assemble_system_one_cell(cell, scratch, data);
        },
        [this](const PerTaskData_ASM &data) {
            this->constraints.distribute_local_to_global(
                data.cell_matrix, data.cell_rhs, data.local_dof_indices,
                tangent_matrix, system_rhs);
        },
        scratch_data, per_task_data);

    timer.leave_subsection();
}

template <int dim>
void Solid<dim>::assemble_system_one_cell(
    const typename DoFHandler<dim>::active_cell_iterator &cell,
    ScratchData_ASM &scratch, PerTaskData_ASM &data) const {
    data.reset();
    scratch.reset();
    scratch.fe_values.reinit(cell);
    cell->get_dof_indices(data.local_dof_indices);

    const std::vector<std::shared_ptr<const PointHistory<dim>>> lqph =
        quadrature_point_history.get_data(cell);
    AssertDimension(lqph.size(), n_q_points);

    for (const unsigned int q_point :
         scratch.fe_values.quadrature_point_indices()) {
        const Tensor<2, dim> F_inv = lqph[q_point]->get_F_inv();
        for (const unsigned int k : scratch.fe_values.dof_indices()) {
            const unsigned int k_group = fe.system_to_base_index(k).first.first;

            if (k_group == u_dof) {
                scratch.grad_Nx[q_point][k] =
                    scratch.fe_values[u_fe].gradient(k, q_point) * F_inv;
                scratch.symm_grad_Nx[q_point][k] =
                    symmetrize(scratch.grad_Nx[q_point][k]);
            } else if (k_group == p_dof)
                scratch.Nx[q_point][k] =
                    scratch.fe_values[p_fe].value(k, q_point);
            else if (k_group == J_dof)
                scratch.Nx[q_point][k] =
                    scratch.fe_values[J_fe].value(k, q_point);
            else
                DEAL_II_ASSERT_UNREACHABLE();
        }
    }

    for (const unsigned int q_point :
         scratch.fe_values.quadrature_point_indices()) {
        const SymmetricTensor<2, dim> tau = lqph[q_point]->get_tau();
        const Tensor<2, dim> tau_ns = lqph[q_point]->get_tau();
        const SymmetricTensor<4, dim> Jc = lqph[q_point]->get_Jc();
        const double det_F = lqph[q_point]->get_det_F();
        const double p_tilde = lqph[q_point]->get_p_tilde();
        const double J_tilde = lqph[q_point]->get_J_tilde();
        const double dPsi_vol_dJ = lqph[q_point]->get_dPsi_vol_dJ();
        const double d2Psi_vol_dJ2 = lqph[q_point]->get_d2Psi_vol_dJ2();
        const SymmetricTensor<2, dim> &I =
            Physics::Elasticity::StandardTensors<dim>::I;

        SymmetricTensor<2, dim> symm_grad_Nx_i_x_Jc;
        Tensor<1, dim> grad_Nx_i_comp_i_x_tau;

        const std::vector<double> &N = scratch.Nx[q_point];
        const std::vector<SymmetricTensor<2, dim>> &symm_grad_Nx =
            scratch.symm_grad_Nx[q_point];
        const std::vector<Tensor<2, dim>> &grad_Nx = scratch.grad_Nx[q_point];
        const double JxW = scratch.fe_values.JxW(q_point);

        for (const unsigned int i : scratch.fe_values.dof_indices()) {
            const unsigned int component_i =
                fe.system_to_component_index(i).first;
            const unsigned int i_group = fe.system_to_base_index(i).first.first;

            if (i_group == u_dof)
                data.cell_rhs(i) -= (symm_grad_Nx[i] * tau) * JxW;
            else if (i_group == p_dof)
                data.cell_rhs(i) -= N[i] * (det_F - J_tilde) * JxW;
            else if (i_group == J_dof)
                data.cell_rhs(i) -= N[i] * (dPsi_vol_dJ - p_tilde) * JxW;
            else
                DEAL_II_ASSERT_UNREACHABLE();

            if (i_group == u_dof) {
                symm_grad_Nx_i_x_Jc = symm_grad_Nx[i] * Jc;
                grad_Nx_i_comp_i_x_tau = grad_Nx[i][component_i] * tau_ns;
            }

            for (const unsigned int j :
                 scratch.fe_values.dof_indices_ending_at(i)) {
                const unsigned int component_j =
                    fe.system_to_component_index(j).first;
                const unsigned int j_group =
                    fe.system_to_base_index(j).first.first;

                if ((i_group == u_dof) && (j_group == u_dof)) // UU block
                {
                    data.cell_matrix(i, j) +=
                        symm_grad_Nx_i_x_Jc * symm_grad_Nx[j] * JxW;

                    if (component_i == component_j)
                        data.cell_matrix(i, j) += grad_Nx_i_comp_i_x_tau *
                                                  grad_Nx[j][component_j] * JxW;
                } else if ((i_group == p_dof) && (j_group == u_dof)) // PU block
                {
                    data.cell_matrix(i, j) +=
                        N[i] * det_F * (symm_grad_Nx[j] * I) * JxW;
                } else if ((i_group == J_dof) && (j_group == p_dof)) // JP block
                    data.cell_matrix(i, j) -= N[i] * N[j] * JxW;
                else if ((i_group == J_dof) && (j_group == J_dof)) // JJ block
                    data.cell_matrix(i, j) += N[i] * d2Psi_vol_dJ2 * N[j] * JxW;
                else if ((i_group <= J_dof) && (j_group <= J_dof)) {
                    /* Nothing to do for the remaining blocks. */
                } else
                    DEAL_II_ASSERT_UNREACHABLE();
            }
        }
    }

    for (const auto &face : cell->face_iterators())
        if (face->at_boundary() && face->boundary_id() == 11) {
            scratch.fe_face_values.reinit(cell, face);

            for (const unsigned int f_q_point :
                 scratch.fe_face_values.quadrature_point_indices()) {
                const Tensor<1, dim> &N =
                    scratch.fe_face_values.normal_vector(f_q_point);

                Tensor<1, dim> dir;
                dir[1] = 0.0625;
                //                const Tensor<1, dim> traction = 1.0 * dir;

                static const double p0 =
                    1.0 / (parameters.scale * parameters.scale);
                const double time_ramp = (time.current() / time.end());
                const double pressure = p0 * parameters.p_p0 * time_ramp;
                const Tensor<1, dim> traction = pressure * dir;

                for (const unsigned int i : scratch.fe_values.dof_indices()) {
                    const unsigned int i_group =
                        fe.system_to_base_index(i).first.first;

                    if (i_group == u_dof) {
                        const unsigned int component_i =
                            fe.system_to_component_index(i).first;
                        const double Ni =
                            scratch.fe_face_values.shape_value(i, f_q_point);
                        const double JxW =
                            scratch.fe_face_values.JxW(f_q_point);

                        data.cell_rhs(i) += (Ni * traction[component_i]) * JxW;
                    }
                }
            }
        }

    for (const unsigned int i : scratch.fe_values.dof_indices())
        for (const unsigned int j :
             scratch.fe_values.dof_indices_starting_at(i + 1))
            data.cell_matrix(i, j) = data.cell_matrix(j, i);
}

template <int dim> void Solid<dim>::make_constraints(const unsigned int it_nr) {
    const bool apply_dirichlet_bc = (it_nr == 0);

    if (it_nr > 1) {
        std::cout << " --- " << std::flush;
        return;
    }

    std::cout << " CST " << std::flush;

    if (apply_dirichlet_bc) {
        constraints.clear();

        //        for (const auto &cell : triangulation.active_cell_iterators())
        //            for (unsigned int f = 0; f < cell->n_faces(); ++f)
        //                if (cell->face(f)->at_boundary()){
        //                    const unsigned int b_id =
        //                    cell->face(f)->boundary_id(); const Point<dim>
        //                    center = cell->face(f)->center();
        //
        //                    std::cout << "Boundary ID: " << b_id
        //                              << " at center = (" << center[0]
        //                              << ", " << center[1];
        //                    if constexpr (dim == 3)
        //                        std::cout << ", " << center[2];
        //                    std::cout << ")" << std::endl;
        //                }

        // TODO: Add dim == 2 case (NOW: Fix leftmost face)

        //        VectorTools::interpolate_boundary_values(
        //            dof_handler, /*boundary_id=*/1,
        //            Functions::ZeroFunction<dim>(n_components), constraints,
        //            fe.component_mask(x_displacement)); // Constrain
        //            y-direction
        //
        //        VectorTools::interpolate_boundary_values(
        //            dof_handler, /*boundary_id=*/1,
        //            Functions::ZeroFunction<dim>(n_components), constraints,
        //            fe.component_mask(y_displacement)); // Constrain
        //            y-direction
        //
        //        VectorTools::interpolate_boundary_values(
        //            dof_handler, /*boundary_id=*/2,
        //            Functions::ZeroFunction<dim>(n_components), constraints,
        //            fe.component_mask(z_displacement)); // Constrain
        //            y-direction

        const FEValuesExtractors::Scalar x_displacement(0);
        const FEValuesExtractors::Scalar y_displacement(1);
        const FEValuesExtractors::Scalar z_displacement(2);

        {
            const int boundary_id = 3;

            VectorTools::interpolate_boundary_values(
                dof_handler, boundary_id,
                Functions::ZeroFunction<dim>(n_components), constraints,
                fe.component_mask(z_displacement));
        }
        {
            const int boundary_id = 1;

            VectorTools::interpolate_boundary_values(
                dof_handler, boundary_id,
                Functions::ZeroFunction<dim>(n_components), constraints,
                fe.component_mask(u_fe));
        }

        {
            const int boundary_id = 2;

            VectorTools::interpolate_boundary_values(
                dof_handler, boundary_id,
                Functions::ZeroFunction<dim>(n_components), constraints,
                fe.component_mask(z_displacement));
        }

    } else {
        if (constraints.has_inhomogeneities()) {
            AffineConstraints<double> homogeneous_constraints(constraints);
            for (unsigned int dof = 0; dof != dof_handler.n_dofs(); ++dof)
                if (homogeneous_constraints.is_inhomogeneously_constrained(dof))
                    homogeneous_constraints.set_inhomogeneity(dof, 0.0);

            constraints.clear();
            constraints.copy_from(homogeneous_constraints);
        }
    }

    constraints.close();
}

template <int dim> void Solid<dim>::assemble_sc() {
    timer.enter_subsection("Perform static condensation");
    std::cout << " ASM_SC " << std::flush;

    PerTaskData_SC per_task_data(dofs_per_cell, element_indices_u.size(),
                                 element_indices_p.size(),
                                 element_indices_J.size());
    ScratchData_SC scratch_data;

    WorkStream::run(dof_handler.active_cell_iterators(), *this,
                    &Solid::assemble_sc_one_cell,
                    &Solid::copy_local_to_global_sc, scratch_data,
                    per_task_data);

    timer.leave_subsection();
}

template <int dim>
void Solid<dim>::copy_local_to_global_sc(const PerTaskData_SC &data) {
    for (unsigned int i = 0; i < dofs_per_cell; ++i)
        for (unsigned int j = 0; j < dofs_per_cell; ++j)
            tangent_matrix.add(data.local_dof_indices[i],
                               data.local_dof_indices[j],
                               data.cell_matrix(i, j));
}

template <int dim>
void Solid<dim>::assemble_sc_one_cell(
    const typename DoFHandler<dim>::active_cell_iterator &cell,
    ScratchData_SC &scratch, PerTaskData_SC &data) {
    data.reset();
    scratch.reset();
    cell->get_dof_indices(data.local_dof_indices);

    data.k_orig.extract_submatrix_from(tangent_matrix, data.local_dof_indices,
                                       data.local_dof_indices);
    data.k_pu.extract_submatrix_from(data.k_orig, element_indices_p,
                                     element_indices_u);
    data.k_pJ.extract_submatrix_from(data.k_orig, element_indices_p,
                                     element_indices_J);
    data.k_JJ.extract_submatrix_from(data.k_orig, element_indices_J,
                                     element_indices_J);

    data.k_pJ_inv.invert(data.k_pJ);

    data.k_pJ_inv.mmult(data.A, data.k_pu);
    data.k_JJ.mmult(data.B, data.A);
    data.k_pJ_inv.Tmmult(data.C, data.B);
    data.k_pu.Tmmult(data.k_bbar, data.C);
    data.k_bbar.scatter_matrix_to(element_indices_u, element_indices_u,
                                  data.cell_matrix);

    data.k_pJ_inv.add(-1.0, data.k_pJ);
    data.k_pJ_inv.scatter_matrix_to(element_indices_p, element_indices_J,
                                    data.cell_matrix);
}

template <int dim>
std::pair<unsigned int, double>
Solid<dim>::solve_linear_system(BlockVector<double> &newton_update) {
    unsigned int lin_it = 0;
    double lin_res = 0.0;

    if (parameters.use_static_condensation == true) {

        BlockVector<double> A(dofs_per_block);
        BlockVector<double> B(dofs_per_block);

        {
            assemble_sc();

            tangent_matrix.block(p_dof, J_dof)
                .vmult(A.block(J_dof), system_rhs.block(p_dof));
            tangent_matrix.block(J_dof, J_dof)
                .vmult(B.block(J_dof), A.block(J_dof));
            A.block(J_dof) = system_rhs.block(J_dof);
            A.block(J_dof) -= B.block(J_dof);
            tangent_matrix.block(p_dof, J_dof)
                .Tvmult(A.block(p_dof), A.block(J_dof));
            tangent_matrix.block(u_dof, p_dof)
                .vmult(A.block(u_dof), A.block(p_dof));
            system_rhs.block(u_dof) -= A.block(u_dof);

            timer.enter_subsection("Linear solver");
            std::cout << " SLV " << std::flush;
            if (parameters.type_lin == "CG") {
                const auto solver_its = static_cast<unsigned int>(
                    tangent_matrix.block(u_dof, u_dof).m() *
                    parameters.max_iterations_lin);
                const double tol_sol =
                    parameters.tol_lin * system_rhs.block(u_dof).l2_norm();

                SolverControl solver_control(solver_its, tol_sol);

                GrowingVectorMemory<Vector<double>> GVM;
                SolverCG<Vector<double>> solver_CG(solver_control, GVM);

                PreconditionSelector<SparseMatrix<double>, Vector<double>>
                    preconditioner(parameters.preconditioner_type,
                                   parameters.preconditioner_relaxation);
                preconditioner.use_matrix(tangent_matrix.block(u_dof, u_dof));

                solver_CG.solve(tangent_matrix.block(u_dof, u_dof),
                                newton_update.block(u_dof),
                                system_rhs.block(u_dof), preconditioner);

                lin_it = solver_control.last_step();
                lin_res = solver_control.last_value();
            } else if (parameters.type_lin == "Direct") {
                SparseDirectUMFPACK A_direct;
                A_direct.initialize(tangent_matrix.block(u_dof, u_dof));
                A_direct.vmult(newton_update.block(u_dof),
                               system_rhs.block(u_dof));

                lin_it = 1;
                lin_res = 0.0;
            } else
                Assert(false, ExcMessage("Linear solver type not implemented"));

            timer.leave_subsection();
        }

        constraints.distribute(newton_update);

        timer.enter_subsection("Linear solver postprocessing");
        std::cout << " PP " << std::flush;

        {
            tangent_matrix.block(p_dof, u_dof)
                .vmult(A.block(p_dof), newton_update.block(u_dof));
            A.block(p_dof) *= -1.0;
            A.block(p_dof) += system_rhs.block(p_dof);
            tangent_matrix.block(p_dof, J_dof)
                .vmult(newton_update.block(J_dof), A.block(p_dof));
        }

        constraints.distribute(newton_update);

        {
            tangent_matrix.block(J_dof, J_dof)
                .vmult(A.block(J_dof), newton_update.block(J_dof));
            A.block(J_dof) *= -1.0;
            A.block(J_dof) += system_rhs.block(J_dof);
            tangent_matrix.block(p_dof, J_dof)
                .Tvmult(newton_update.block(p_dof), A.block(J_dof));
        }

        constraints.distribute(newton_update);

        timer.leave_subsection();
    } else {
        std::cout << " ------ " << std::flush;

        timer.enter_subsection("Linear solver");
        std::cout << " SLV " << std::flush;

        if (parameters.type_lin == "CG") {

            const Vector<double> &f_u = system_rhs.block(u_dof);
            const Vector<double> &f_p = system_rhs.block(p_dof);
            const Vector<double> &f_J = system_rhs.block(J_dof);

            Vector<double> &d_u = newton_update.block(u_dof);
            Vector<double> &d_p = newton_update.block(p_dof);
            Vector<double> &d_J = newton_update.block(J_dof);

            const auto K_uu =
                linear_operator(tangent_matrix.block(u_dof, u_dof));
            const auto K_up =
                linear_operator(tangent_matrix.block(u_dof, p_dof));
            const auto K_pu =
                linear_operator(tangent_matrix.block(p_dof, u_dof));
            const auto K_Jp =
                linear_operator(tangent_matrix.block(J_dof, p_dof));
            const auto K_JJ =
                linear_operator(tangent_matrix.block(J_dof, J_dof));

            PreconditionSelector<SparseMatrix<double>, Vector<double>>
                preconditioner_K_Jp_inv("jacobi");
            preconditioner_K_Jp_inv.use_matrix(
                tangent_matrix.block(J_dof, p_dof));
            ReductionControl solver_control_K_Jp_inv(
                static_cast<unsigned int>(
                    tangent_matrix.block(J_dof, p_dof).m() *
                    parameters.max_iterations_lin),
                1.0e-30, parameters.tol_lin);
            SolverSelector<Vector<double>> solver_K_Jp_inv;
            solver_K_Jp_inv.select("cg");
            solver_K_Jp_inv.set_control(solver_control_K_Jp_inv);
            const auto K_Jp_inv = inverse_operator(K_Jp, solver_K_Jp_inv,
                                                   preconditioner_K_Jp_inv);

            const auto K_pJ_inv = transpose_operator(K_Jp_inv);
            const auto K_pp_bar = K_Jp_inv * K_JJ * K_pJ_inv;
            const auto K_uu_bar_bar = K_up * K_pp_bar * K_pu;
            const auto K_uu_con = K_uu + K_uu_bar_bar;

            PreconditionSelector<SparseMatrix<double>, Vector<double>>
                preconditioner_K_con_inv(parameters.preconditioner_type,
                                         parameters.preconditioner_relaxation);
            preconditioner_K_con_inv.use_matrix(
                tangent_matrix.block(u_dof, u_dof));
            ReductionControl solver_control_K_con_inv(
                static_cast<unsigned int>(
                    tangent_matrix.block(u_dof, u_dof).m() *
                    parameters.max_iterations_lin),
                1.0e-30, parameters.tol_lin);
            SolverSelector<Vector<double>> solver_K_con_inv;
            solver_K_con_inv.select("cg");
            solver_K_con_inv.set_control(solver_control_K_con_inv);
            const auto K_uu_con_inv = inverse_operator(
                K_uu_con, solver_K_con_inv, preconditioner_K_con_inv);

            d_u =
                K_uu_con_inv * (f_u - K_up * (K_Jp_inv * f_J - K_pp_bar * f_p));

            timer.leave_subsection();

            timer.enter_subsection("Linear solver postprocessing");
            std::cout << " PP " << std::flush;

            d_J = K_pJ_inv * (f_p - K_pu * d_u);
            d_p = K_Jp_inv * (f_J - K_JJ * d_J);

            lin_it = solver_control_K_con_inv.last_step();
            lin_res = solver_control_K_con_inv.last_value();
        } else if (parameters.type_lin == "Direct") {
            SparseDirectUMFPACK A_direct;
            A_direct.initialize(tangent_matrix);
            A_direct.vmult(newton_update, system_rhs);

            lin_it = 1;
            lin_res = 0.0;

            std::cout << " -- " << std::flush;
        } else
            Assert(false, ExcMessage("Linear solver type not implemented"));

        timer.leave_subsection();

        constraints.distribute(newton_update);
    }

    return std::make_pair(lin_it, lin_res);
}

template <int dim> void Solid<dim>::output_results() const {
    DataOut<dim> data_out;
    std::vector<DataComponentInterpretation::DataComponentInterpretation>
        data_component_interpretation(
            dim, DataComponentInterpretation::component_is_part_of_vector);
    data_component_interpretation.push_back(
        DataComponentInterpretation::component_is_scalar);
    data_component_interpretation.push_back(
        DataComponentInterpretation::component_is_scalar);

    std::vector<std::string> solution_name(dim, "displacement");
    solution_name.emplace_back("pressure");
    solution_name.emplace_back("dilatation");

    DataOutBase::VtkFlags output_flags;
    output_flags.write_higher_order_cells = true;
    output_flags.physical_units["displacement"] = "m";
    data_out.set_flags(output_flags);

    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution_n, solution_name,
                             DataOut<dim>::type_dof_data,
                             data_component_interpretation);

    Vector<double> stress_norm(triangulation.n_active_cells());
    unsigned int counter = 0;
    for (const auto &cell : triangulation.active_cell_iterators()) {
        double accumulated_norm = 0.0;
        const auto lqph = quadrature_point_history.get_data(cell);
        for (unsigned int q = 0; q < n_q_points; ++q)
            accumulated_norm += lqph[q]->get_tau().norm();

        stress_norm[counter++] = accumulated_norm / n_q_points;
    }
    data_out.add_data_vector(stress_norm, "stress_norm");

    Vector<double> soln(solution_n.size());
    for (unsigned int i = 0; i < soln.size(); ++i)
        soln(i) = solution_n(i);

    const MappingQEulerian<dim> q_mapping(degree, dof_handler, soln);

    Vector<double> displacement;
    displacement.reinit(dof_handler.n_dofs());

    for (unsigned int i = 0; i < dof_handler.n_dofs(); ++i)
        displacement[i] = solution_n[i];

    data_out.build_patches(q_mapping, degree);

    const auto &patches = data_out.get_patches();
    double max_y = -std::numeric_limits<double>::max();
    Point<dim> max_point;
    for (const auto &patch : patches) {

        for (const auto &vertex : patch.vertices) {
            if (vertex[1] > max_y) {
                max_y = vertex[1];
                max_point = vertex;
            }
        }
    }
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Heightest position when deformed state: " << max_point << std::endl;
    std::ofstream output("solution-" + std::to_string(dim) + "d-" +
                         std::to_string(time.get_timestep()) + ".vtu");
    data_out.write_vtu(output);
}

} // namespace MLSolver

int main() {
    using namespace MLSolver;

    try {
        const unsigned int dim = 3;

        Solid<dim> solid("../../parameters.prm");
        solid.run();
    } catch (std::exception &exc) {
        std::cerr << std::endl
                  << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        std::cerr << "Exception on processing: " << std::endl
                  << exc.what() << std::endl
                  << "Aborting!" << std::endl
                  << "----------------------------------------------------"
                  << std::endl;

        return 1;
    } catch (...) {
        std::cerr << std::endl
                  << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        std::cerr << "Unknown exception!" << std::endl
                  << "Aborting!" << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        return 1;
    }

    return 0;
}

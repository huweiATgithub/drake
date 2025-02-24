#include "drake/multibody/plant/sap_driver.h"

#include <gtest/gtest.h>

#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/common/test_utilities/expect_throws_message.h"
#include "drake/math/rigid_transform.h"
#include "drake/multibody/contact_solvers/contact_solver_results.h"
#include "drake/multibody/contact_solvers/contact_solver_utils.h"
#include "drake/multibody/contact_solvers/sap/sap_contact_problem.h"
#include "drake/multibody/contact_solvers/sap/sap_friction_cone_constraint.h"
#include "drake/multibody/contact_solvers/sap/sap_solver.h"
#include "drake/multibody/contact_solvers/sap/sap_solver_results.h"
#include "drake/multibody/plant/compliant_contact_manager.h"
#include "drake/multibody/plant/multibody_plant.h"
#include "drake/multibody/plant/test/compliant_contact_manager_tester.h"
#include "drake/multibody/plant/test/spheres_stack.h"
#include "drake/multibody/tree/body.h"
#include "drake/multibody/tree/revolute_joint.h"

using drake::math::RigidTransformd;
using drake::multibody::Body;
using drake::multibody::RevoluteJoint;
using drake::multibody::contact_solvers::internal::ContactSolverResults;
using drake::multibody::contact_solvers::internal::MergeNormalAndTangent;
using drake::multibody::contact_solvers::internal::SapContactProblem;
using drake::multibody::contact_solvers::internal::SapFrictionConeConstraint;
using drake::multibody::contact_solvers::internal::SapSolver;
using drake::multibody::contact_solvers::internal::SapSolverParameters;
using drake::multibody::contact_solvers::internal::SapSolverResults;
using drake::multibody::internal::DiscreteContactPair;
using drake::systems::Context;
using Eigen::MatrixXd;
using Eigen::Vector3d;
using Eigen::VectorXd;

// TODO(amcastro-tri): Implement AutoDiffXd testing.

namespace drake {
namespace multibody {
namespace internal {

constexpr double kEps = std::numeric_limits<double>::epsilon();

// Friend class used to provide access to a selection of private functions in
// SapDriver for testing purposes.
class SapDriverTest {
 public:
  static const ContactProblemCache<double>& EvalContactProblemCache(
      const SapDriver<double>& driver, const Context<double>& context) {
    return driver.EvalContactProblemCache(context);
  }

  static VectorXd CalcFreeMotionVelocities(const SapDriver<double>& driver,
                                           const Context<double>& context) {
    VectorXd v_star(driver.plant().num_velocities());
    driver.CalcFreeMotionVelocities(context, &v_star);
    return v_star;
  }

  static std::vector<MatrixXd> CalcLinearDynamicsMatrix(
      const SapDriver<double>& driver, const Context<double>& context) {
    std::vector<MatrixXd> A;
    driver.CalcLinearDynamicsMatrix(context, &A);
    return A;
  }

  static void PackContactSolverResults(
      const SapDriver<double>& driver, const Context<double>& context,
      const contact_solvers::internal::SapContactProblem<double>& problem,
      int num_contacts,
      const contact_solvers::internal::SapSolverResults<double>& sap_results,
      contact_solvers::internal::ContactSolverResults<double>*
          contact_results) {
    driver.PackContactSolverResults(context, problem, num_contacts, sap_results,
                                    contact_results);
  }
};

// Test fixture to test the functionality provided by SapDriver, with the
// problem configuration implemented in SpheresStack (refer to that testing
// class for details on the configuration.)
class SpheresStackTest : public SpheresStack, public ::testing::Test {
 public:
  const SapDriver<double>& sap_driver() const {
    return CompliantContactManagerTester::sap_driver(*contact_manager_);
  }

  // The functions below provide access to private SapDriver functions for unit
  // testing.

  const ContactProblemCache<double>& EvalContactProblemCache(
      const Context<double>& context) const {
    return SapDriverTest::EvalContactProblemCache(sap_driver(), context);
  }

  VectorXd CalcFreeMotionVelocities(const Context<double>& context) const {
    VectorXd v_star(plant_->num_velocities());
    return SapDriverTest::CalcFreeMotionVelocities(sap_driver(), context);
  }

  std::vector<MatrixXd> CalcLinearDynamicsMatrix(
      const Context<double>& context) const {
    return SapDriverTest::CalcLinearDynamicsMatrix(sap_driver(), context);
  }

  void PackContactSolverResults(
      const Context<double>& context,
      const contact_solvers::internal::SapContactProblem<double>& problem,
      int num_contacts,
      const contact_solvers::internal::SapSolverResults<double>& sap_results,
      contact_solvers::internal::ContactSolverResults<double>* contact_results)
      const {
    SapDriverTest::PackContactSolverResults(sap_driver(), context, problem,
                                            num_contacts, sap_results,
                                            contact_results);
  }

  // The functions below provide access to private CompliantContactManager
  // functions for unit testing.

  const internal::MultibodyTreeTopology& topology() const {
    return CompliantContactManagerTester::topology(*contact_manager_);
  }

  const std::vector<DiscreteContactPair<double>>& EvalDiscreteContactPairs(
      const Context<double>& context) const {
    return CompliantContactManagerTester::EvalDiscreteContactPairs(
        *contact_manager_, context);
  }

  std::vector<ContactPairKinematics<double>> CalcContactKinematics(
      const Context<double>& context) const {
    return CompliantContactManagerTester::CalcContactKinematics(
        *contact_manager_, context);
  }
};

// This test verifies that the SapContactProblem built by the driver is
// consistent with the contact kinematics computed with CalcContactKinematics().
TEST_F(SpheresStackTest, EvalContactProblemCache) {
  SetupRigidGroundCompliantSphereAndNonHydroSphere();
  const ContactProblemCache<double>& problem_cache =
      EvalContactProblemCache(*plant_context_);
  const SapContactProblem<double>& problem = *problem_cache.sap_problem;
  const std::vector<drake::math::RotationMatrix<double>>& R_WC =
      problem_cache.R_WC;

  const std::vector<DiscreteContactPair<double>>& pairs =
      EvalDiscreteContactPairs(*plant_context_);
  const int num_contacts = pairs.size();

  // Verify sizes.
  EXPECT_EQ(problem.num_cliques(), topology().num_trees());
  EXPECT_EQ(problem.num_velocities(), plant_->num_velocities());
  EXPECT_EQ(problem.num_constraints(), num_contacts);
  EXPECT_EQ(problem.num_constraint_equations(), 3 * num_contacts);
  EXPECT_EQ(problem.time_step(), plant_->time_step());
  ASSERT_EQ(R_WC.size(), num_contacts);

  // Verify dynamics data.
  const VectorXd& v_star = CalcFreeMotionVelocities(*plant_context_);
  const std::vector<MatrixXd>& A = CalcLinearDynamicsMatrix(*plant_context_);
  EXPECT_EQ(problem.v_star(), v_star);
  EXPECT_EQ(problem.dynamics_matrix(), A);

  // Verify each of the contact constraints.
  const std::vector<ContactPairKinematics<double>> contact_kinematics =
      CalcContactKinematics(*plant_context_);
  for (size_t i = 0; i < contact_kinematics.size(); ++i) {
    const DiscreteContactPair<double>& discrete_pair = pairs[i];
    const ContactPairKinematics<double>& pair_kinematics =
        contact_kinematics[i];
    const auto* constraint =
        dynamic_cast<const SapFrictionConeConstraint<double>*>(
            &problem.get_constraint(i));
    // In this test we do know all constraints are contact constraints.
    ASSERT_NE(constraint, nullptr);
    EXPECT_EQ(constraint->num_cliques(), pair_kinematics.jacobian.size());
    EXPECT_EQ(constraint->first_clique(), pair_kinematics.jacobian[0].tree);
    EXPECT_EQ(constraint->first_clique_jacobian().MakeDenseMatrix(),
              pair_kinematics.jacobian[0].J.MakeDenseMatrix());
    if (constraint->num_cliques() == 2) {
      EXPECT_EQ(constraint->second_clique(), pair_kinematics.jacobian[1].tree);
      EXPECT_EQ(constraint->second_clique_jacobian().MakeDenseMatrix(),
                pair_kinematics.jacobian[1].J.MakeDenseMatrix());
    }
    EXPECT_EQ(constraint->parameters().mu, discrete_pair.friction_coefficient);
    EXPECT_EQ(constraint->parameters().stiffness, discrete_pair.stiffness);
    EXPECT_EQ(constraint->parameters().dissipation_time_scale,
              discrete_pair.dissipation_time_scale);
    EXPECT_EQ(constraint->parameters().beta,
              plant_->get_sap_near_rigid_threshold());
    // This parameter sigma is for now hard-code in the manager to these value.
    // Here we simply test they are consistent with those hard-coded values.
    EXPECT_EQ(constraint->parameters().sigma, 1.0e-3);

    // Verify contact configuration.
    const contact_solvers::internal::ContactConfiguration<double>&
        configuration = pair_kinematics.configuration;
    EXPECT_EQ(constraint->configuration().objectA, configuration.objectA);
    EXPECT_EQ(constraint->configuration().objectB, configuration.objectB);
    EXPECT_EQ(constraint->configuration().p_ApC_W, configuration.p_ApC_W);
    EXPECT_EQ(constraint->configuration().p_BqC_W, configuration.p_BqC_W);
    EXPECT_EQ(constraint->configuration().phi, configuration.phi);
    EXPECT_EQ(constraint->configuration().R_WC.matrix(), R_WC[i].matrix());
  }
}

// Verifies the correctness of the computation of free motion velocities when
// external forces are applied.
TEST_F(SpheresStackTest, CalcFreeMotionVelocitiesWithExternalForces) {
  SetupRigidGroundCompliantSphereAndNonHydroSphere();

  // We set an arbitrary non-zero external force to the plant to verify it gets
  // properly applied as part of the computation.
  const int nv = plant_->num_velocities();
  const VectorXd tau = VectorXd::LinSpaced(nv, 1.0, 2.0);
  plant_->get_applied_generalized_force_input_port().FixValue(plant_context_,
                                                              tau);

  // Set arbitrary non-zero velocities.
  const VectorXd v0 = VectorXd::LinSpaced(nv, 2.0, 3.0);
  plant_->SetVelocities(plant_context_, v0);

  // Since the spheres's frames are located at their COM and since their
  // rotational inertias are triaxially symmetric, the Coriolis term is zero.
  // Therefore the momentum equation reduces to: M * (v-v0)/dt = tau_g + tau.
  const double dt = plant_->time_step();
  const VectorXd tau_g = plant_->CalcGravityGeneralizedForces(*plant_context_);
  MatrixXd M(nv, nv);
  plant_->CalcMassMatrix(*plant_context_, &M);
  const VectorXd v_expected = v0 + dt * M.ldlt().solve(tau_g + tau);

  // Compute the velocities the system would have next time step in the absence
  // of constraints.
  const VectorXd v_star = CalcFreeMotionVelocities(*plant_context_);

  EXPECT_TRUE(
      CompareMatrices(v_star, v_expected, kEps, MatrixCompareType::relative));
}

// Verifies that joint limit forces are applied.
TEST_F(SpheresStackTest, CalcFreeMotionVelocitiesWithJointLimits) {
  // In this model sphere 1 is attached to the world by a prismatic joint with
  // lower limit z = 0.
  const bool sphere1_on_prismatic_joint = true;
  SetupRigidGroundCompliantSphereAndNonHydroSphere(sphere1_on_prismatic_joint);

  const int nv = plant_->num_velocities();

  // Set arbitrary non-zero velocities.
  const VectorXd non_zero_vs = VectorXd::LinSpaced(nv, 2.0, 3.0);
  plant_->SetVelocities(plant_context_, non_zero_vs);

  // The slider velocity is set to be negative to ensure the joint limit is
  // active. That is, the position was set to be below the lower limit and in
  // addition it is decreasing.
  slider1_->set_translation_rate(plant_context_, -1.0);

  // Initial velocities.
  const VectorXd v0 = plant_->GetVelocities(*plant_context_);

  // Compute slider1's velocity in the absence of joint limits. This is
  // equivalent to computing the free motion velocities before constraints are
  // applied.
  const VectorXd v_expected = CalcFreeMotionVelocities(*plant_context_);
  const double v_slider_no_limits = v_expected(slider1_->velocity_start());

  // Compute slider1's velocity with constraints applied. This corresponds to
  // the full discrete update computation.
  ContactSolverResults<double> contact_results;
  contact_manager_->CalcContactSolverResults(*plant_context_, &contact_results);
  const VectorXd v_star = contact_results.v_next;
  const double v_slider_star = v_star(slider1_->velocity_start());

  // While other solver specific tests verify the correctness of joint limit
  // forces, this test is simply limited to verifying the manager applied them.
  // Therefore we only check the force limits have the effect of making the
  // slider velocity larger than if not present.
  EXPECT_GT(v_slider_star, v_slider_no_limits);
}

TEST_F(SpheresStackTest, CalcLinearDynamicsMatrix) {
  SetupRigidGroundCompliantSphereAndNonHydroSphere();
  const std::vector<MatrixXd> A = CalcLinearDynamicsMatrix(*plant_context_);
  const int nv = plant_->num_velocities();
  MatrixXd Adense = MatrixXd::Zero(nv, nv);
  for (TreeIndex t(0); t < topology().num_trees(); ++t) {
    const int tree_start = topology().tree_velocities_start(t);
    const int tree_nv = topology().num_tree_velocities(t);
    Adense.block(tree_start, tree_start, tree_nv, tree_nv) = A[t];
  }
  MatrixXd Aexpected(nv, nv);
  plant_->CalcMassMatrix(*plant_context_, &Aexpected);
  EXPECT_TRUE(
      CompareMatrices(Adense, Aexpected, kEps, MatrixCompareType::relative));
}

// Here we test the function CompliantContactManager::PackContactSolverResults()
// which takes SapSolverResults and packs them into ContactSolverResults as
// consumed by MultibodyPlant.
TEST_F(SpheresStackTest, PackContactSolverResults) {
  SetupRigidGroundCompliantSphereAndNonHydroSphere();

  // We form an arbitrary set of SAP results consistent with the contact
  // kinematics for the configuration of our model.
  const std::vector<ContactPairKinematics<double>> contact_kinematics =
      CalcContactKinematics(*plant_context_);
  const int num_contacts = contact_kinematics.size();
  const int nv = plant_->num_velocities();
  SapSolverResults<double> sap_results;
  sap_results.Resize(nv, 3 * num_contacts);
  sap_results.v = VectorXd::LinSpaced(nv, -3.0, 14.0);
  sap_results.gamma = VectorXd::LinSpaced(3 * num_contacts, -12.0, 8.0);
  sap_results.vc = VectorXd::LinSpaced(3 * num_contacts, -1.0, 11.0);
  // Not used to pack contact results.
  sap_results.j = VectorXd::Constant(nv, NAN);

  // Pack SAP results into contact results.
  const SapContactProblem<double>& sap_problem =
      *EvalContactProblemCache(*plant_context_).sap_problem;
  ContactSolverResults<double> contact_results;
  PackContactSolverResults(*plant_context_, sap_problem, num_contacts,
                           sap_results, &contact_results);

  // Verify against expected values.
  VectorXd gamma(3 * num_contacts);
  MergeNormalAndTangent(contact_results.fn, contact_results.ft, &gamma);
  gamma *= plant_->time_step();
  EXPECT_TRUE(CompareMatrices(gamma, sap_results.gamma, kEps,
                              MatrixCompareType::relative));
  VectorXd vc(3 * num_contacts);
  MergeNormalAndTangent(contact_results.vn, contact_results.vt, &vc);
  EXPECT_TRUE(
      CompareMatrices(vc, sap_results.vc, kEps, MatrixCompareType::relative));
  const MatrixXd J_AcBc_C =
      CompliantContactManagerTester::CalcDenseJacobianMatrixInContactFrame(
          *contact_manager_, contact_kinematics);
  const VectorXd tau_expected =
      J_AcBc_C.transpose() * sap_results.gamma / plant_->time_step();
  EXPECT_TRUE(CompareMatrices(contact_results.tau_contact, tau_expected,
                              2.0 * kEps, MatrixCompareType::relative));
}

// Unit test that the manager throws an exception whenever SAP fails to
// converge.
TEST_F(SpheresStackTest, SapFailureException) {
  SetupRigidGroundCompliantSphereAndNonHydroSphere();
  ContactSolverResults<double> contact_results;
  // To trigger SAP's failure, we limit the maximum number of iterations to
  // zero.
  SapSolverParameters parameters;
  parameters.max_iterations = 0;
  contact_manager_->set_sap_solver_parameters(parameters);
  DRAKE_EXPECT_THROWS_MESSAGE(contact_manager_->CalcContactSolverResults(
                                  *plant_context_, &contact_results),
                              "The SAP solver failed to converge(.|\n)*");
}

// Unit test that the manager is forwarded the active status of each constraint
// and produces a SapContactProblem with only the active constraints and
// recalculates the cached SapContactProblem with constraint parameters change.
GTEST_TEST(SapDriverTest, ConstraintActiveStatus) {
  MultibodyPlant<double> plant(0.01);
  plant.set_discrete_contact_solver(DiscreteContactSolver::kSap);

  // Add two bodies and two joints in order to cover all constraint types.
  // We aren't doing any physics in this test, just checking that constraint
  // active status is forwarded through to the driver. So bodies, joints, and
  // constraints have arbitrary non-physical parameters for ease of setup.
  const Body<double>& body_A =
      plant.AddRigidBody("body_A", SpatialInertia<double>::MakeUnitary());
  const Body<double>& body_B =
      plant.AddRigidBody("body_B", SpatialInertia<double>::MakeUnitary());
  const RevoluteJoint<double>& world_A = plant.AddJoint<RevoluteJoint>(
      "world_A", plant.world_body(), RigidTransformd::Identity(), body_A,
      RigidTransformd::Identity(), Vector3d::UnitZ());
  const RevoluteJoint<double>& A_B = plant.AddJoint<RevoluteJoint>(
      "A_B", body_A, RigidTransformd::Identity(), body_B,
      RigidTransformd::Identity(), Vector3d::UnitZ());

  // Add one coupler constraint, one distance constraint, and one ball
  // constraint.
  MultibodyConstraintId coupler_constraint_id =
      plant.AddCouplerConstraint(world_A, A_B, 1.2);
  MultibodyConstraintId distance_constraint_id = plant.AddDistanceConstraint(
      body_A, Vector3d::Zero(), body_B, Vector3d(1.0, 2.0, 3.0), 1.0);
  MultibodyConstraintId ball_constraint_id = plant.AddBallConstraint(
      body_A, Vector3d::Zero(), body_B, Vector3d::Zero());

  // Finalize and set the manager/driver.
  plant.Finalize();

  auto owned_contact_manager =
      std::make_unique<CompliantContactManager<double>>();
  const CompliantContactManager<double>* contact_manager =
      owned_contact_manager.get();
  plant.SetDiscreteUpdateManager(std::move(owned_contact_manager));
  const SapDriver<double>& sap_driver =
      CompliantContactManagerTester::sap_driver(*contact_manager);

  std::unique_ptr<Context<double>> context = plant.CreateDefaultContext();

  // All constraints active.
  {
    const ContactProblemCache<double>& problem_cache =
        SapDriverTest::EvalContactProblemCache(sap_driver, *context);
    const SapContactProblem<double>& problem = *problem_cache.sap_problem;

    // Verify number of constraints when all are active. There is no contact
    // so we expect one coupler constraint, one distance constraint and one
    // ball constraint.
    EXPECT_EQ(problem.num_constraints(), 3);
    EXPECT_EQ(problem.num_constraint_equations(),
              1 /* coupler constraint */ + 1 /* distance constraint */ +
                  3 /* ball constraint */);
    // TODO(joemasterjohn): When these constraints become first class citizens,
    // verify the constraints via type casts rather than just looking at the
    // number of constraint equations as a proxy.
    EXPECT_EQ(problem.get_constraint(0).num_constraint_equations(), 1);
    EXPECT_EQ(problem.get_constraint(1).num_constraint_equations(), 1);
    EXPECT_EQ(problem.get_constraint(2).num_constraint_equations(), 3);
  }

  // Set the distance and ball constraint to inactive and verify that only the
  // coupler constraint shows up in the problem.
  {
    plant.SetConstraintActiveStatus(context.get(), distance_constraint_id,
                                    false);
    plant.SetConstraintActiveStatus(context.get(), ball_constraint_id, false);

    const ContactProblemCache<double>& problem_cache =
        SapDriverTest::EvalContactProblemCache(sap_driver, *context);
    const SapContactProblem<double>& problem = *problem_cache.sap_problem;

    // Verify number of constraints when only the coupler constraint is active.
    EXPECT_EQ(problem.num_constraints(), 1);
    EXPECT_EQ(problem.num_constraint_equations(), 1 /* coupler constraint */);
    EXPECT_EQ(problem.get_constraint(0).num_constraint_equations(), 1);
  }

  // Now disable the coupler constraint and verify that the problem has 0
  // constraints.
  {
    plant.SetConstraintActiveStatus(context.get(), coupler_constraint_id,
                                    false);

    const ContactProblemCache<double>& problem_cache =
        SapDriverTest::EvalContactProblemCache(sap_driver, *context);
    const SapContactProblem<double>& problem = *problem_cache.sap_problem;

    // Verify number of constraints when all are inactive.
    EXPECT_EQ(problem.num_constraints(), 0);
    EXPECT_EQ(problem.num_constraint_equations(), 0);
  }

  // Reactivate all constraints and verify they show up in the problem.
  {
    plant.SetConstraintActiveStatus(context.get(), distance_constraint_id,
                                    true);
    plant.SetConstraintActiveStatus(context.get(), ball_constraint_id, true);
    plant.SetConstraintActiveStatus(context.get(), coupler_constraint_id, true);

    const ContactProblemCache<double>& problem_cache =
        SapDriverTest::EvalContactProblemCache(sap_driver, *context);
    const SapContactProblem<double>& problem = *problem_cache.sap_problem;

    // Verify number of constraints when all are active. There is no contact
    // so we expect one coupler constraint, one distance constraint and one
    // ball constraint.
    EXPECT_EQ(problem.num_constraints(), 3);
    EXPECT_EQ(problem.num_constraint_equations(),
              1 /* coupler constraint */ + 1 /* distance constraint */ +
                  3 /* ball constraint */);
    // TODO(joemasterjohn): When these constraints become first class citizens,
    // verify the constraints via type casts rather than just looking at the
    // number of constraint equations as a proxy.
    EXPECT_EQ(problem.get_constraint(0).num_constraint_equations(), 1);
    EXPECT_EQ(problem.get_constraint(1).num_constraint_equations(), 1);
    EXPECT_EQ(problem.get_constraint(2).num_constraint_equations(), 3);
  }
}

}  // namespace internal
}  // namespace multibody
}  // namespace drake

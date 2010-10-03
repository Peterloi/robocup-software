/*
 * RbpfModel.hpp
 *
 *  General overview:
 *  A particle model forms a node in the model graph that handles the 'linear'
 *  portions of the filtering. This is implemented as an Extended Kalman Filter
 *  (EKF) and this model contains everything from an EKF except for the state
 *  and covariance themselves, which are maintained within the particle filter.
 *
 *  Note:
 *    when possible the notation here is based on:
 *      http://en.wikipedia.org/wiki/Kalman_filter
 *      http://en.wikipedia.org/wiki/Particle_filter
 *
 *  Author: Philip Rogers, Nov 15th 2009
 */

#ifndef RBPFMODEL_HPP_
#define RBPFMODEL_HPP_

#include <iostream>
#include <fstream>
#include <assert.h>
#include <LinearAlgebra.hpp>
#include "../RobotModel.hpp"

// Constants for state sizes across all models
// If these are changed, make sure to modify the virtual functions, such as
// transitionModel(), which are written assuming specific state sizes
#define NSIZE 6 // size of state
#define MSIZE 6 // size of control input
#define SSIZE 2 // size of measurement

class RbpfModel {
public:
	// robotMap is a set of robots for calculating kicks/deflections in some models
	RbpfModel(Modeling::RobotModel::RobotMap *robotMap);

	// Used for printing this model to a stream (debugging)
	friend std::ostream& operator<<(std::ostream& out, const RbpfModel &model);

	// performs EKF predict, storing the result in X and P
	// X: state vector that will be updated (n x 1)
	// P: state covariance that will be updated (n x n)
	// U: control input (m x 1)
	// dt: change in time
	void predict(LinAlg::Vector &X, LinAlg::Matrix &P, LinAlg::Vector &U, double dt);

	// performs EKF update, storing the result in X and P
	// X: state vector that will be updated (n x 1)
	// P: state covariance that will be updated (n x n)
	// Z: observation (s x 1)
	// dt: change in time
	virtual void update(LinAlg::Vector &X, LinAlg::Matrix &P, LinAlg::Vector &Z, double dt);

	// functions that pull new values in from config files
	virtual void initializeQ()=0;
	virtual void initializeR()=0;

	// returns the previously predicted measurement, h (s x 1)
	LinAlg::Vector* getPredictedMeasurement();

	// returns the previously calculated innovation, Yhat (s x 1)
	LinAlg::Vector* getInnovation();

	// returns the previously calculated innovation, S (s x s)
	LinAlg::Matrix* getInnovationCovariance();

	const int n;    // size of state
	const int m;    // size of control input
	const int s;    // size of measurement

protected:
	// Function: transitionModel(&X,&U)
	//   X: state vector that will be updated (n x 1)
	//   U: control input (m x 1)
	//   dt: change in time
	virtual void transitionModel(LinAlg::Vector &X, LinAlg::Vector &U, double dt) = 0;

	// Computes the transition Jacobian and stores the result in F
	// Must call before predict()
	virtual void computeTransitionJacobian(double dt) = 0;

	// X: state vector (n x 1)
	// out: observation (s x 1)
	virtual void observationModel(LinAlg::Vector &X, LinAlg::Vector &out) = 0;

	// computes the Jacobian of the observation Model function, wrt the state
	// and stores the result in H.
	// Must call before update()
	virtual void computeObservationJacobian(double dt) = 0;

	LinAlg::Matrix F; // state transition Jacobian (df/dx) (n x n)
	LinAlg::Matrix H; // observation Jacobian (dh/dx) (s x n)
	LinAlg::Matrix Q; // process noise (n x n)
	LinAlg::Matrix R; // measurement noise (s x s)
	Modeling::RobotModel::RobotMap *_robotMap; // set of robots for kicks/deflections

	// variables used in intermediate calculations
	LinAlg::Matrix Inn;  // identity matrix (n x n)
	LinAlg::Vector h;    // predicted measurement (s x 1)
	LinAlg::Vector Yhat; // innovation (s x 1)
	LinAlg::Matrix S;    // innovation (or residual) covariance (s x s)
};

#endif /* RBPFMODEL_HPP_ */
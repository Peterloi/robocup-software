#include "MotionControl.hpp"
#include <SystemState.hpp>
#include <RobotConfig.hpp>
#include <Robot.hpp>
#include <Utils.hpp>
#include "TrapezoidalMotion.hpp"

#include <cmath>
#include <stdio.h>
#include <algorithm>

using namespace std;
using namespace Geometry2d;


#pragma mark Config Variables

REGISTER_CONFIGURABLE(MotionControl);

ConfigDouble *MotionControl::_pid_vel_p;
ConfigDouble *MotionControl::_pid_vel_i;
ConfigInt    * MotionControl::_pid_vel_i_windup;
// ConfigDouble *MotionControl::_pid_vel_d;
ConfigDouble *MotionControl::_vel_mult;

ConfigDouble *MotionControl::_pid_pos_p;

ConfigDouble *MotionControl::_pid_angle_p;
ConfigDouble *MotionControl::_pid_angle_i;
ConfigDouble *MotionControl::_pid_angle_d;
ConfigDouble *MotionControl::_angle_vel_mult;
// ConfigDouble *MotionControl::_max_angle_w;

ConfigDouble *MotionControl::_max_acceleration;
ConfigDouble *MotionControl::_max_velocity;

ConfigDouble *MotionControl::_path_jitter_compensation_factor;

ConfigDouble *MotionControl::_pivot_vel_multiplier;

void MotionControl::createConfiguration(Configuration *cfg) {
	_pid_vel_p = new ConfigDouble(cfg, "MotionControl/pos/vel_PID_p", 6.5);
	_pid_vel_i = new ConfigDouble(cfg, "MotionControl/pos/vel_PID_i", 0.0001);
	_pid_vel_i_windup = new ConfigInt(cfg, "MotionControl/pos/vel_PID_i_windup", 0);
	// _pid_vel_d = new ConfigDouble(cfg, "MotionControl/pos/vel_PID_d", 0);
	_vel_mult = new ConfigDouble(cfg, "MotionControl/pos/Velocity Multiplier", 1);
	_pid_pos_p = new ConfigDouble(cfg, "MotionControl/pos/PID_p", 1);

	_pid_angle_p	= new ConfigDouble(cfg, "MotionControl/angle/PID_p", 1);
	_pid_angle_i	= new ConfigDouble(cfg, "MotionControl/angle/PID_i", 0.00001);
	_pid_angle_d	= new ConfigDouble(cfg, "MotionControl/angle/PID_d", 0.001);
	_angle_vel_mult	= new ConfigDouble(cfg, "MotionControl/angle/Velocity Multiplier", 0.5);
	// _max_angle_w	= new ConfigDouble(cfg, "MotionControl/angle/Max w", 10);

	_max_acceleration	= new ConfigDouble(cfg, "MotionControl/Max Acceleration", 1.5);
	_max_velocity		= new ConfigDouble(cfg, "MotionControl/Max Velocity", 2.0);

	_path_jitter_compensation_factor = new ConfigDouble(cfg, "MotionControl/PathJitterCompensationFactor", 2.5);

	_pivot_vel_multiplier = new ConfigDouble(cfg, "MotionControl/PivotVelMultiplier", 05);
}


#pragma mark MotionControl

MotionControl::MotionControl(OurRobot *robot) : _angleController(0, 0, 0, 50) {
	_robot = robot;

	_robot->radioTx.set_robot_id(_robot->shell());
	_lastCmdTime = -1;
}


void MotionControl::run() {
	if (!_robot) return;

	const MotionConstraints &constraints = _robot->motionConstraints();

	//	update PID parameters
	_velXController.kp = *_pid_vel_p;
	_velXController.ki = *_pid_vel_i;
	_velXController.setWindup(*_pid_vel_i_windup);
	_velXController.kd = 0;	//*_pid_vel_d;

	_velYController.kp = *_pid_vel_p;
	_velYController.ki = *_pid_vel_i;
	_velYController.setWindup(*_pid_vel_i_windup);
	_velYController.kd = 0;	//*_pid_vel_d;

	_posXController.kp = *_pid_pos_p;
	_posXController.ki = 0;
	_posXController.kd = 0;

	_posYController.kp = *_pid_pos_p;
	_posYController.ki = 0;
	_posYController.kd = 0;

	_angleController.kp = *_pid_angle_p;
	_angleController.ki = *_pid_angle_i;
	_angleController.kd = *_pid_angle_d;



	//	Angle control //////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	float targetW = 0;
	if (constraints.targetAngleVel) {
		targetW = *constraints.targetAngleVel;
	} else if (constraints.faceTarget || constraints.pivotTarget) {
		const Geometry2d::Point &targetPt = constraints.pivotTarget ? *constraints.pivotTarget : *constraints.faceTarget;

		float targetAngleFinal = (targetPt - _robot->pos).angle() * RadiansToDegrees;
		float angleError = targetAngleFinal - _robot->angle;

		//	don't go the long way around to get to our final angle
		while (angleError > 180) {
			angleError -= 360;
		} 
		while (angleError < -180) {
			angleError += 360;
		}


		// float targetW;
		// float targetAngle;
		// TrapezoidalMotion(
		// 	abs(angleError),					//	dist
		// 	motionConstraints.maxAngleSpeed,	//	max deg/sec
		// 	30,									//	max deg/sec^2
		// 	0.1 ,								//	time into path
		// 	_robot->angleVel,					//	start speed
		// 	0,									//	final speed
		// 	targetAngle,
		// 	targetW 							//	ignored
		// 	);


		// //	PID on angle
		// if(angleError<0) {
		// 	targetW = - targetW;
		// }
		// targetW = _angleController.run(targetAngle);


		targetW = _angleController.run(angleError);


		//	limit W
		if (abs(targetW) > (constraints.maxAngleSpeed)) {
			if (targetW > 0) {
				targetW = (constraints.maxAngleSpeed);
			} else {
				targetW = -(constraints.maxAngleSpeed);
			}
		}
		
		/*
		_robot->addText(QString("targetW: %1").arg(targetW));
		_robot->addText(QString("angleError: %1").arg(angleError));
		_robot->addText(QString("targetGlobalAngle: %1").arg(targetAngleFinal));
		_robot->addText(QString("angle: %1").arg(_robot->angle));
		*/
	}

	_targetAngleVel(targetW);


	// handle body velocity for pivot command
	if (constraints.pivotTarget) {
		float r = Robot_Radius;
		const float FudgeFactor = *_pivot_vel_multiplier;
		float speed = r * targetW * RadiansToDegrees * FudgeFactor;
		Point vel(speed, 0);

		//	the robot body coordinate system is wierd...
		vel.rotate(-90);

		_targetBodyVel(vel);

		return; //	pivot handles both angle and position
	}



	//	Position control ///////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	//	the velocity command we're about to send to the bot
	Point controlVel;

	//	if no target position is given, we don't have a path to follow
	if (!_robot->path()) {
		if (!constraints.targetWorldVel) {
			controlVel = Point(0, 0);
		} else {
			controlVel = constraints.targetWorldVel->rotated(-_robot->angle);
		}
	} else {
		//
		//	Path following
		//
		
		
		//	convert from microseconds to seconds
		float timeIntoPath = ((float)(timestamp() - _robot->pathStartTime())) * TimestampToSecs + 1.0/60.0;

		//	if the path is getting rapidly changed, we cheat so that the robot actually moves
		//	see OurRobot._recentPathChangeTimes for more info
		// if (_robot->isRepeatedlyChangingPaths()) {
		// 	timeIntoPath = max<float>(timeIntoPath, OurRobot::PathChangeHistoryBufferSize * 1.0f/60.0f * 0.8);
		// 	cout << "Compensating!  new t = " << timeIntoPath << endl;
		// }

		//	the 0.9 is a fudge factor
		//	we do this to compensate for lost command cycles
		double factor = *_path_jitter_compensation_factor;
		timeIntoPath += _robot->consecutivePathChangeCount() * 1.0f/60.0f * factor;


		//	evaluate path - where should we be right now?
		Point targetPos, targetVel;
		bool pathValidNow = _robot->path()->evaluate(
			timeIntoPath,
			targetPos,
			targetVel);
		if (!pathValidNow) {
			targetVel.x = 0;
			targetVel.y = 0;
			if (_robot->path()->destination()) {
				targetPos = *(_robot->path()->destination());
			}
		}

		//	error
		Point posError = targetPos - _robot->pos;
		Point velError = targetVel - _robot->vel;

		//	run cascaded P PI controller
		Point velCtl = Point(_velXController.run(velError.x + _posXController.run(posError.x)),
			_velYController.run(velError.y + _posYController.run(posError.y)));

					// + Point(_velXController.run(velError.x + _posXController.run(posError.x)),
					// 		_velYController.run(velError.y + _posYController.run(posError.y)));
					// // + Point(_posXController.run(posError.x), _posYController.run(posError.y));

		cout << "target: <" << targetVel.x << ", " << targetVel.y << ">" << endl;
		cout << "velCtl: <" << velCtl.x << ", " << velCtl.y << ">" << endl;

		controlVel = targetVel + velCtl;


		//	draw target pt
		_robot->state()->drawCircle(targetPos, .04, Qt::red, "MotionControl");
		_robot->state()->drawLine(targetPos, targetPos + controlVel, Qt::blue, "velocity");
		// _robot->state()->drawText(QString("%1").arg(timeIntoPath), targetPos, Qt::black, "time");

		//	convert from world to body coordinates
		controlVel = controlVel.rotated(-_robot->angle);
	}

	this->_targetBodyVel(controlVel);
}

void MotionControl::stopped() {
	_targetBodyVel(Point(0, 0));
	_targetAngleVel(0);
}

void MotionControl::_targetAngleVel(float angleVel) {
	//	velocity multiplier
	angleVel *= *_angle_vel_mult;

	_robot->radioTx.set_body_w(angleVel);
}

void MotionControl::_targetBodyVel(Point targetVel) {
	// Limit Velocity
	targetVel.clamp(*_max_velocity);

	// Limit Acceleration
	if (_lastCmdTime == -1) {
		targetVel.clamp(*_max_acceleration);
	} else {
		float dt = (float)((timestamp() - _lastCmdTime) / 1000000.0f);
		Point targetAccel = (targetVel - _lastVelCmd) / dt ;
		targetAccel.clamp(*_max_acceleration);

		targetVel = _lastVelCmd + targetAccel * dt;
	}

	//	make sure we don't send any bad values
	if (isnan(targetVel.x) || isnan(targetVel.y)) {
		targetVel = Point(0,0);
	}

	//	track these values so we can limit acceleration
	_lastVelCmd = targetVel;
	_lastCmdTime = timestamp();

	//	velocity multiplier
	targetVel *= *_vel_mult;

	//	set radioTx values
	_robot->radioTx.set_body_x(targetVel.x);
	_robot->radioTx.set_body_y(targetVel.y);
}

// -*- LSST-C++ -*-

/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010, 2011 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

#include "Eigen/Eigenvalues"
#include "Eigen/SVD"
#include "Eigen/Cholesky"
#include "boost/format.hpp"
#include "boost/make_shared.hpp"

#include "lsst/afw/math/LeastSquares.h"
#include "lsst/pex/exceptions.h"

namespace lsst { namespace afw { namespace math {

class LeastSquares::Impl {
public:

    enum HessianState { NO_HESSIAN=0, LOWER_HESSIAN=1, FULL_HESSIAN=2 };

    HessianState hessianState;
    double threshold;
    int dimension;
    int rank;

    Eigen::MatrixXd design;
    Eigen::VectorXd data;
    Eigen::MatrixXd hessian;
    Eigen::VectorXd rhs;

    ndarray::Array<double,1,1> solution;
    ndarray::Array<double,2,2> covariance;

    template <typename D>
    void setRank(Eigen::MatrixBase<D> const & values) {
        double cond = threshold * values[0];
        for (rank = dimension; (rank > 0) && (values[rank-1] < cond); --rank);
    }

    void computeHessian(HessianState desired) {
        if (hessianState < LOWER_HESSIAN && desired > LOWER_HESSIAN) {
            hessian = Eigen::MatrixXd::Zero(design.cols(), design.cols());
            hessian.selfadjointView<Eigen::Lower>().rankUpdate(design.adjoint());
            hessianState = LOWER_HESSIAN;
        }
        if (hessianState < FULL_HESSIAN && desired >= FULL_HESSIAN) {
            hessian.triangularView<Eigen::StrictlyUpper>() = hessian.adjoint();
            hessianState = FULL_HESSIAN;
        }
    }

    virtual void factor() = 0;

    virtual void updateRank();

    virtual void solve() = 0;
    virtual void computeCovariance() = 0;

    Impl(int dimension_, double threshold_) : 
        hessianState(NO_HESSIAN), threshold(threshold_), dimension(dimension_), rank(dimension_)
        {}

    virtual ~Impl() {}
};

namespace {

class EigensystemSolver : public LeastSquares::Impl {
public:

    explicit EigensystemSolver(int dimension) :
        Impl(dimension, std::sqrt(std::numeric_limits<double>::epsilon())),
        _eig(dimension), _svd()
        {}
    
    virtual void factor() {
        computeHessian(LOWER_HESSIAN);
        _eig.compute(hessian);
        if (_eig.info() == Eigen::Success) {
            setRank(_eig.eigenvalues());
        } else {
            _svd.compute(hessian, Eigen::ComputeFullU); // Matrix is symmetric, so V == U
            setRank(_svd.singularValues());
        }
    }

    virtual void updateRank() {
        if (_eig.info() == Eigen::Success) {
            setRank(_eig.eigenvalues());
        } else {
            setRank(_svd.singularValues());
        }
    }

    virtual void solve() {
        if (_eig.info() == Eigen::Success) {
            _tmp.head(rank) = _eig.eigenvectors().leftCols(rank).adjoint() * rhs;
            _tmp.head(rank).array() /= _eig.eigenvalues().head(rank).array();
            solution.asEigen() = _eig.eigenvectors().leftCols(rank) * _tmp;
        } else {
            _tmp.head(rank) = _svd.matrixU().leftCols(rank).adjoint() * rhs;
            _tmp.head(rank).array() /= _svd.singularValues().head(rank).array();
            solution.asEigen() = _svd.matrixU().leftCols(rank) * _tmp;
        }
    }

    virtual void computeCovariance() {
        if (_eig.info() == Eigen::Success) {
            covariance.asEigen() = 
                _eig.eigenvectors().leftCols(rank)
                * _eig.eigenvalues().head(rank).asDiagonal()
                * _eig.eigenvectors().leftCols(rank).adjoint();
        } else {
            covariance.asEigen() = 
                _svd.matrixU().leftCols(rank)
                * _svd.singularValues().head(rank).asDiagonal()
                * _svd.matrixU().leftCols(rank).adjoint();
        }
    }
        
private:
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> _eig;
    Eigen::JacobiSVD<Eigen::MatrixXd> _svd; // only used if Eigendecomposition fails, should be very rare
    Eigen::VectorXd _tmp;
};

class CholeskySolver : public LeastSquares::Impl {
public:

    explicit CholeskySolver(int dimension) : Impl(dimension, 0.0), _ldlt(dimension) {}
    
    virtual void factor() { computeHessian(LOWER_HESSIAN); _ldlt.compute(hessian); }

    virtual void updateRank() {}

    virtual void solve() { solution.asEigen() = _ldlt.solve(rhs); }

    virtual void computeCovariance() {
        ndarray::EigenView<double,2,2> cov(covariance);
        cov.setIdentity();
        cov = _ldlt.solve(cov);
    }
        
private:
    Eigen::LDLT<Eigen::MatrixXd> _ldlt;
};

} // anonymous

void LeastSquares::setThreshold(double threshold) { _impl->threshold = threshold; _impl->updateRank(); }

double LeastSquares::getThreshold() const { return _impl->threshold; }

ndarray::Array<double const,1,1> LeastSquares::solve() {
    if (_impl->solution.isEmpty()) {
        _impl->solution = ndarray::allocate(_impl->dimension);
    }
    _impl->solve();
    return _impl->solution;
}

ndarray::Array<double const,2,2> LeastSquares::computeCovariance() {
    if (_impl->covariance.isEmpty()) {
        _impl->covariance = ndarray::allocate(_impl->dimension, _impl->dimension);
    }
    _impl->computeCovariance();
    return _impl->covariance;
}

ndarray::Array<double const,2,2> LeastSquares::computeHessian() {
    _impl->computeHessian(Impl::FULL_HESSIAN);
    // Wrap the Eigen::MatrixXd in an ndarray::Array, using _impl as the reference-counted owner.
    // Doesn't matter if we swap strides, because it's symmetric.
    return ndarray::external(
        _impl->hessian.data(),
        ndarray::makeVector(_impl->dimension, _impl->dimension),
        ndarray::makeVector(_impl->dimension, 1),
        _impl
    );
}

int LeastSquares::getDimension() const { return _impl->dimension; }

int LeastSquares::getRank() const { return _impl->rank; }

LeastSquares::LeastSquares(Factorization factorization, int dimension) {
    switch (factorization) {
    case NORMAL_EIGENSYSTEM:
        _impl = boost::make_shared<EigensystemSolver>(dimension);
        break;
    case NORMAL_CHOLESKY:
        _impl = boost::make_shared<CholeskySolver>(dimension);
        break;
    case DIRECT_SVD:
        //_impl = boost::make_shared<SvdSolver>(dimension);
        break;        
    }
}

LeastSquares::~LeastSquares() {}

Eigen::MatrixXd & LeastSquares::_getDesignMatrix() { return _impl->design; }
Eigen::VectorXd & LeastSquares::_getDataVector() { return _impl->data; }

Eigen::MatrixXd & LeastSquares::_getHessianMatrix() { return _impl->hessian; }
Eigen::VectorXd & LeastSquares::_getRhsVector() { return _impl->rhs; }

void LeastSquares::_factor(bool haveNormalEquations) {
    if (haveNormalEquations) {
        if (_getHessianMatrix().rows() != _impl->dimension) {
            throw LSST_EXCEPT(
                pex::exceptions::InvalidParameterException,
                (boost::format("Number of rows of Hessian matrix (%d) does not match"
                               " dimension of LeastSquares solver.")
                 % _getHessianMatrix().rows() % _impl->dimension).str()
            );
        }
        if (_getHessianMatrix().cols() != _impl->dimension) {
            throw LSST_EXCEPT(
                pex::exceptions::InvalidParameterException,
                (boost::format("Number of columns of Hessian matrix (%d) does not match"
                               " dimension of LeastSquares solver.")
                 % _getHessianMatrix().cols() % _impl->dimension).str()
            );
        }
        if (_getRhsVector().size() != _impl->dimension) {
            throw LSST_EXCEPT(
                pex::exceptions::InvalidParameterException,
                (boost::format("Number of elements in RHS vector (%d) does not match"
                               " dimension of LeastSquares solver.")
                 % _getRhsVector().size() % _impl->dimension).str()
            );
        }
        _impl->hessianState = Impl::FULL_HESSIAN;
    } else {
        if (_getDesignMatrix().cols() != _impl->dimension) {
            throw LSST_EXCEPT(
                pex::exceptions::InvalidParameterException,
                "Number of columns of design matrix does not match dimension of LeastSquares solver."
            );
        }
        if (_getDesignMatrix().rows() != _getDataVector().size()) {
            throw LSST_EXCEPT(
                pex::exceptions::InvalidParameterException,
                (boost::format("Number of rows of design matrix (%d) does not match number of "
                               "data points (%d)") % _getDesignMatrix().rows() % _getDataVector().size()
                ).str()
            );
        }
        _impl->hessianState = Impl::NO_HESSIAN;
    }
    _impl->factor();
}

}}} // namespace lsst::afw::math

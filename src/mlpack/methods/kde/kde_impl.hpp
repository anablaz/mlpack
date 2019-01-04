/**
 * @file kde_impl.hpp
 * @author Roberto Hueso (robertohueso96@gmail.com)
 *
 * Implementation of Kernel Density Estimation.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */

#include "kde.hpp"
#include "kde_rules.hpp"

namespace mlpack {
namespace kde {

//! Construct tree that rearranges the dataset
template<typename TreeType, typename MatType>
TreeType* BuildTree(
    MatType&& dataset,
    std::vector<size_t>& oldFromNew,
    const typename std::enable_if<
        tree::TreeTraits<TreeType>::RearrangesDataset>::type* = 0)
{
  return new TreeType(std::forward<MatType>(dataset), oldFromNew);
}

//! Construct tree that doesn't rearrange the dataset
template<typename TreeType, typename MatType>
TreeType* BuildTree(
    MatType&& dataset,
    const std::vector<size_t>& /* oldFromNew */,
    const typename std::enable_if<
        !tree::TreeTraits<TreeType>::RearrangesDataset>::type* = 0)
{
  return new TreeType(std::forward<MatType>(dataset));
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
KDE() :
    kernel(new KernelType()),
    metric(new MetricType()),
    relError(0.05),
    absError(0.0),
    ownsKernel(true),
    ownsMetric(true),
    ownsReferenceTree(false),
    trained(false),
    mode(DUAL_TREE_MODE) { }

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
KDE(const double bandwidth,
    const double relError,
    const double absError,
    const KDEMode mode) :
    kernel(new KernelType(bandwidth)),
    metric(new MetricType()),
    relError(relError),
    absError(absError),
    ownsKernel(true),
    ownsMetric(true),
    ownsReferenceTree(false),
    trained(false),
    mode(mode)
{
  CheckErrorValues(relError, absError);
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
KDE(MetricType& metric,
    KernelType& kernel,
    const double relError,
    const double absError,
    const KDEMode mode) :
    kernel(&kernel),
    metric(&metric),
    relError(relError),
    absError(absError),
    ownsKernel(false),
    ownsMetric(false),
    ownsReferenceTree(false),
    trained(false),
    mode(mode)
{
  CheckErrorValues(relError, absError);
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
KDE(const KDE& other) :
    kernel(new KernelType(other.kernel)),
    metric(new MetricType(other.metric)),
    relError(other.relError),
    absError(other.absError),
    ownsKernel(other.ownsKernel),
    ownsMetric(other.ownsMetric),
    ownsReferenceTree(other.ownsReferenceTree),
    trained(other.trained),
    mode(other.mode)
{
  if (trained)
  {
    if (ownsReferenceTree)
    {
      oldFromNewReferences = new std::vector<size_t>;
      referenceTree = new Tree(other.referenceTree, *oldFromNewReferences);
    }
    else
    {
      oldFromNewReferences = other.oldFromNewReferences;
      referenceTree = other.referenceTree;
    }
  }
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
KDE(KDE&& other) :
    kernel(other.kernel),
    metric(other.metric),
    referenceTree(other.referenceTree),
    oldFromNewReferences(other.oldFromNewReferences),
    relError(other.relError),
    absError(other.absError),
    ownsKernel(other.ownsKernel),
    ownsMetric(other.ownsMetric),
    ownsReferenceTree(other.ownsReferenceTree),
    trained(other.trained),
    mode(other.mode)
{
  other.kernel = new KernelType();
  other.metric = new MetricType();
  other.referenceTree = nullptr;
  other.oldFromNewReferences = nullptr;
  other.ownsReferenceTree = false;
  other.trained = false;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>&
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
operator=(KDE other)
{
  // Clean memory
  if (ownsKernel)
    delete kernel;
  if (ownsMetric)
    delete metric;
  if (ownsReferenceTree)
  {
    delete referenceTree;
    delete oldFromNewReferences;
  }

  // Move
  this->kernel = std::move(other.kernel);
  this->metric = std::move(other.metric);
  this->referenceTree = std::move(other.referenceTree);
  this->oldFromNewReferences = std::move(other.oldFromNewReferences);
  this->relError = other.relError;
  this->absError = other.absError;
  this->ownsKernel = other.ownsKernel;
  this->ownsMetric = other.ownsMetric;
  this->ownsReferenceTree = other.ownsReferenceTree;
  this->trained = other.trained;
  this->mode = other.mode;

  return *this;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
KDE<MetricType,
    MatType,
    KernelType,
    TreeType,
    DualTreeTraversalType,
    SingleTreeTraversalType>::
~KDE()
{
  if (ownsKernel)
    delete kernel;
  if (ownsMetric)
    delete metric;
  if (ownsReferenceTree)
  {
    delete referenceTree;
    delete oldFromNewReferences;
  }
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
Train(MatType referenceSet)
{
  // Check if referenceSet is not an empty set.
  if (referenceSet.n_cols == 0)
    throw std::invalid_argument("cannot train KDE model with an empty "
                                "reference set");
  if (ownsReferenceTree)
  {
    delete referenceTree;
    delete oldFromNewReferences;
  }
  this->ownsReferenceTree = true;
  Timer::Start("building_reference_tree");
  this->oldFromNewReferences = new std::vector<size_t>;
  this->referenceTree = BuildTree<Tree>(std::move(referenceSet),
                                        *oldFromNewReferences);
  Timer::Stop("building_reference_tree");
  this->trained = true;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
Train(Tree* referenceTree, std::vector<size_t>* oldFromNewReferences)
{
  // Check if referenceTree dataset is not an empty set.
  if (referenceTree->Dataset().n_cols == 0)
    throw std::invalid_argument("cannot train KDE model with an empty "
                                "reference set");
  if (ownsReferenceTree == true)
  {
    delete this->referenceTree;
    delete this->oldFromNewReferences;
  }
  this->ownsReferenceTree = false;
  this->referenceTree = referenceTree;
  this->oldFromNewReferences = oldFromNewReferences;
  this->trained = true;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
Evaluate(MatType querySet, arma::vec& estimations)
{
  if (mode == DUAL_TREE_MODE)
  {
    Timer::Start("building_query_tree");
    std::vector<size_t> oldFromNewQueries;
    Tree* queryTree = BuildTree<Tree>(std::move(querySet), oldFromNewQueries);
    Timer::Stop("building_query_tree");
    this->Evaluate(queryTree, oldFromNewQueries, estimations);
    delete queryTree;
  }
  else if (mode == SINGLE_TREE_MODE)
  {
    // Get estimations vector ready.
    estimations.clear();
    estimations.set_size(querySet.n_cols);
    estimations.fill(arma::fill::zeros);

    // Check querySet has at least 1 element to evaluate.
    if (querySet.n_cols == 0)
    {
      Log::Warn << "KDE::Evaluate(): querySet is empty, no predictions will "
                << "be returned" << std::endl;
      return;
    }
    // Check whether dimensions match.
    if (querySet.n_rows != referenceTree->Dataset().n_rows)
      throw std::invalid_argument("cannot evaluate KDE model: querySet and "
                                  "referenceSet dimensions don't match");

    // Evaluate
    typedef KDERules<MetricType, KernelType, Tree> RuleType;
    RuleType rules = RuleType(referenceTree->Dataset(),
                              querySet,
                              estimations,
                              relError,
                              absError,
                              *metric,
                              *kernel,
                              false);

    // Create traverser.
    SingleTreeTraversalType<RuleType> traverser(rules);

    // Traverse for each point.
    for (size_t i = 0; i < querySet.n_cols; ++i)
      traverser.Traverse(i, *referenceTree);

    estimations /= referenceTree->Dataset().n_cols;
    Timer::Stop("computing_kde");

    Log::Info << rules.Scores() << " node combinations were scored."
              << std::endl;
    Log::Info << rules.BaseCases() << " base cases were calculated."
              << std::endl;
  }
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
Evaluate(Tree* queryTree,
         const std::vector<size_t>& oldFromNewQueries,
         arma::vec& estimations)
{
  // Get estimations vector ready.
  estimations.clear();
  estimations.set_size(queryTree->Dataset().n_cols);
  estimations.fill(arma::fill::zeros);

  // Check querySet has at least 1 element to evaluate.
  if (queryTree->Dataset().n_cols == 0)
  {
    Log::Warn << "KDE::Evaluate(): querySet is empty, no predictions will "
              << "be returned" << std::endl;
    return;
  }
  // Check whether dimensions match.
  if (queryTree->Dataset().n_rows != referenceTree->Dataset().n_rows)
    throw std::invalid_argument("cannot evaluate KDE model: querySet and "
                                "referenceSet dimensions don't match");
  // Check the mode is correct.
  if (mode != DUAL_TREE_MODE)
    throw std::invalid_argument("cannot evaluate KDE model: cannot use "
                                "a query tree when mode is different from "
                                "dual-tree");

  Timer::Start("computing_kde");

  // Evaluate
  typedef KDERules<MetricType, KernelType, Tree> RuleType;
  RuleType rules = RuleType(referenceTree->Dataset(),
                            queryTree->Dataset(),
                            estimations,
                            relError,
                            absError,
                            *metric,
                            *kernel,
                            false);

  // Create traverser.
  DualTreeTraversalType<RuleType> traverser(rules);
  traverser.Traverse(*queryTree, *referenceTree);
  estimations /= referenceTree->Dataset().n_cols;
  Timer::Stop("computing_kde");

  // Rearrange if necessary.
  RearrangeEstimations(oldFromNewQueries, estimations);

  Log::Info << rules.Scores() << " node combinations were scored." << std::endl;
  Log::Info << rules.BaseCases() << " base cases were calculated." << std::endl;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
Evaluate(arma::vec& estimations)
{
  // Get estimations vector ready.
  estimations.clear();
  estimations.set_size(referenceTree->Dataset().n_cols);
  estimations.fill(arma::fill::zeros);

  Timer::Start("computing_kde");
  // Evaluate
  typedef KDERules<MetricType, KernelType, Tree> RuleType;
  RuleType rules = RuleType(referenceTree->Dataset(),
                            referenceTree->Dataset(),
                            estimations,
                            relError,
                            absError,
                            *metric,
                            *kernel,
                            true);

  if (mode == DUAL_TREE_MODE)
  {
    // Create traverser.
    DualTreeTraversalType<RuleType> traverser(rules);
    traverser.Traverse(*referenceTree, *referenceTree);
  }
  else if (mode == SINGLE_TREE_MODE)
  {
    SingleTreeTraversalType<RuleType> traverser(rules);
    for (size_t i = 0; i < referenceTree->Dataset().n_cols; ++i)
      traverser.Traverse(i, *referenceTree);
  }

  estimations /= referenceTree->Dataset().n_cols;
  // Rearrange if necessary.
  RearrangeEstimations(*oldFromNewReferences, estimations);
  Timer::Stop("computing_kde");

  Log::Info << rules.Scores() << " node combinations were scored." << std::endl;
  Log::Info << rules.BaseCases() << " base cases were calculated." << std::endl;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
RelativeError(const double newError)
{
  CheckErrorValues(newError, absError);
  relError = newError;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
AbsoluteError(const double newError)
{
  CheckErrorValues(relError, newError);
  absError = newError;
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
template<typename Archive>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
serialize(Archive& ar, const unsigned int /* version */)
{
  // Serialize preferences.
  ar & BOOST_SERIALIZATION_NVP(relError);
  ar & BOOST_SERIALIZATION_NVP(absError);
  ar & BOOST_SERIALIZATION_NVP(trained);
  ar & BOOST_SERIALIZATION_NVP(mode);

  // If we are loading, clean up memory if necessary.
  if (Archive::is_loading::value)
  {
    if (ownsKernel && kernel)
      delete kernel;
    if (ownsMetric && metric)
      delete metric;
    if (ownsReferenceTree && referenceTree)
    {
      delete referenceTree;
      delete oldFromNewReferences;
    }
    // After loading kernel, metric and tree, we own it.
    ownsKernel = true;
    ownsMetric = true;
    ownsReferenceTree = true;
  }

  // Serialize the rest of values.
  ar & BOOST_SERIALIZATION_NVP(kernel);
  ar & BOOST_SERIALIZATION_NVP(metric);
  ar & BOOST_SERIALIZATION_NVP(referenceTree);
  ar & BOOST_SERIALIZATION_NVP(oldFromNewReferences);
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
CheckErrorValues(const double relError, const double absError)
{
  if (relError < 0 || relError > 1)
    throw std::invalid_argument("Relative error tolerance must be a value "
                                "between 0 and 1");
  if (absError < 0)
    throw std::invalid_argument("Absolute error tolerance must be a value "
                                "greater or equal to 0");
}

template<typename MetricType,
         typename MatType,
         typename KernelType,
         template<typename TreeMetricType,
                  typename TreeStatType,
                  typename TreeMatType> class TreeType,
         template<typename> class DualTreeTraversalType,
         template<typename> class SingleTreeTraversalType>
void KDE<MetricType,
         MatType,
         KernelType,
         TreeType,
         DualTreeTraversalType,
         SingleTreeTraversalType>::
RearrangeEstimations(const std::vector<size_t>& oldFromNew,
                     arma::vec& estimations)
{
  if (tree::TreeTraits<Tree>::RearrangesDataset)
  {
    const size_t n_queries = oldFromNew.size();
    arma::vec rearranged_estimations(n_queries);
    for (size_t i = 0; i < n_queries; ++i)
      rearranged_estimations(oldFromNew.at(i)) = estimations(i);
    estimations = std::move(rearranged_estimations);
  }
}

} // namespace kde
} // namespace mlpack

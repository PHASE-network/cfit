
#include <cfit/models/decay3body.hh>
#include <cfit/function.hh>
#include <cfit/random.hh>

Decay3Body::Decay3Body( const Variable&   mSq12,
			const Variable&   mSq13,
			const Variable&   mSq23,
			const Amplitude&  amp  ,
			const PhaseSpace& ps     )
  : DecayModel( mSq12, mSq13, mSq23, amp, ps ), _norm( 1.0 ), _maxPdf( 14.0 )
{
  // Do calculations common to all values of variables
  //    (usually compute norm).
  cache();
}


Decay3Body* Decay3Body::copy() const
{
  return new Decay3Body( *this );
}



const double Decay3Body::evaluateFuncs( const double& mSq12, const double& mSq13, const double& mSq23 ) const
{
  double value = 1.0;

  const std::string& name12 = getVar( 0 ).name(); // mSq12
  const std::string& name13 = getVar( 1 ).name(); // mSq13
  const std::string& name23 = getVar( 2 ).name(); // mSq23

  typedef std::vector< Function >::const_iterator fIter;

  std::map< std::string, double > varMap;
  for ( fIter func = _funcs.begin(); func != _funcs.end(); ++func )
  {
    if ( func->dependsOn( name12 ) ) varMap[ name12 ] = mSq12;
    if ( func->dependsOn( name13 ) ) varMap[ name13 ] = mSq13;
    if ( func->dependsOn( name23 ) ) varMap[ name23 ] = mSq23;

    value *= func->evaluate( varMap );
  }

  // Always return a non-negative value. Default to zero.
  return std::max( value, 0.0 );
}



void Decay3Body::cache()
{
  // Compute the value of _norm.
  _norm = 0.0;

  // Define the properties of the integration method.
  const int    nBins = 400;
  const double min   = _ps.mSq12min();
  const double max   = _ps.mSq12max();
  const double step  = ( max - min ) / double( nBins );

  const double mSqSum = _ps.mSqSum();

  // Define the variables at each bin.
  double mSq12;
  double mSq13;
  double mSq23;

  // Compute the integral on the grid.
  for ( int binX = 0; binX < nBins; ++binX )
    for ( int binY = 0; binY < nBins; ++binY )
    {
      mSq12 = min + step * ( binX + 0.5 );
      mSq13 = min + step * ( binY + 0.5 );
      mSq23 = mSqSum - mSq12 - mSq13;

      // Proceed only if the point lies inside the kinematically allowed Dalitz region.
      // std::norm returns the squared modulus of the complex number, not its norm.
      if ( _ps.contains( mSq12, mSq13, mSq23 ) )
        _norm += std::norm( _amp.evaluate( _ps, mSq12, mSq13, mSq23 ) ) * evaluateFuncs( mSq12, mSq13, mSq23 );
    }

  _norm *= std::pow( step, 2 );

  return;
}



const double Decay3Body::evaluate( const double& mSq12, const double& mSq13, const double& mSq23 ) const throw( PdfException )
{
  // Phase space amplitude of the decay of the particle.
  std::complex< double > amp = _amp.evaluate( _ps, mSq12, mSq13, mSq23 );

  // std::norm returns the squared modulus of the complex number, not its norm.
  return std::norm( amp ) * evaluateFuncs( mSq12, mSq13, mSq23 ) / _norm;
}


const double Decay3Body::evaluate( const double& mSq12, const double& mSq13 ) const throw( PdfException )
{
  const double& mSq23 = _ps.mSqSum() - mSq12 - mSq13;

  // Phase space amplitude of the decay of the particle.
  std::complex< double > amp = _amp.evaluate( _ps, mSq12, mSq13, mSq23 );

  // std::norm returns the squared modulus of the complex number, not its norm.
  return std::norm( amp ) * evaluateFuncs( mSq12, mSq13, mSq23 ) / _norm;
}


const double Decay3Body::project( const std::string& varName, const double& x ) const throw( PdfException )
{
  // Find the index of the variable to be projected.
  int index = -1;
  for ( unsigned var = 0; var < 3; ++var )
    if ( varName == getVar( var ).name() )
      index = var;

  // If the pdf does not depend on the passed variable name, the projection is 1.
  if ( index == -1 )
    return 1.0;

  // Minimum and maximum values of the variable wrt which the integration,
  //    is done i.e. the next to the projected variable.
  const double& min = _ps.mSqMin( ( index + 1 ) % 3 );
  const double& max = _ps.mSqMax( ( index + 1 ) % 3 );

  // Integrate the model.
  const int& nbins = 400;
  double proj = 0.0;
  for ( int yBin = 0; yBin < nbins; ++yBin )
  {
    double y = binCenter( yBin, nbins, min, max );
    double z = _ps.mSqSum() - x - y;

    if ( index == 0 ) proj += evaluate( x, y, z );
    if ( index == 1 ) proj += evaluate( z, x, y );
    if ( index == 2 ) proj += evaluate( y, z, x );
  }

  proj *= ( max - min ) / double( nbins );

  return proj;
}


const double Decay3Body::evaluate( const std::vector< double >& vars ) const throw( PdfException )
{
  std::size_t size = vars.size();

  if ( size == 2 )
    return evaluate( vars[ 0 ], vars[ 1 ] );

  if ( size == 3 )
    return evaluate( vars[ 0 ], vars[ 1 ], vars[ 2 ] );

  throw PdfException( "Decay3Body can only take either 2 or 3 arguments." );
}


// No need to append an operator, since it can only be multiplication.
const Decay3Body& Decay3Body::operator*=( const Function& right ) throw( PdfException )
{
  // Check that the function does not depend on any variables that the model does not.
  const std::map< std::string, Variable >& varMap = right.getVarMap();
  for ( std::map< std::string, Variable >::const_iterator var = varMap.begin(); var != varMap.end(); ++var )
    if ( ! _varMap.count( var->second.name() ) )
      throw PdfException( "Cannot multiply a Decay3Body pdf model by a function that depends on other variables." );

  // Consider the function parameters as own ones.
  const std::map< std::string, Parameter >& parMap = right.getParMap();
  _parMap.insert( parMap.begin(), parMap.end() );

  // Append the function to the functions vector.
  _funcs.push_back( right );

  // Recompute the norm, since the pdf shape has changed under this operation.
  cache();

  return *this;
}



const Decay3Body operator*( Decay3Body left, const Function& right )
{
  // Check that the function does not depend on any variables that the model does not.
  const std::map< std::string, Variable >& varMap = right.getVarMap();
  for ( std::map< std::string, Variable >::const_iterator var = varMap.begin(); var != varMap.end(); ++var )
    if ( ! left._varMap.count( var->second.name() ) )
      throw PdfException( "Cannot multiply a Decay3Body pdf model by a function that depends on other variables." );

  // Consider the function parameters as own ones.
  const std::map< std::string, Parameter >& parMap = right.getParMap();
  left._parMap.insert( parMap.begin(), parMap.end() );

  // Append the function to the functions vector.
  left._funcs.push_back( right );

  // Recompute the norm, since the pdf shape has changed under this operation.
  left.cache();

  return left;
}



const Decay3Body operator*( const Function& left, Decay3Body right )
{
  // Check that the function does not depend on any variables that the model does not.
  const std::map< std::string, Variable >& varMap = left.getVarMap();
  for ( std::map< std::string, Variable >::const_iterator var = varMap.begin(); var != varMap.end(); ++var )
    if ( ! right._varMap.count( var->second.name() ) )
      throw PdfException( "Cannot multiply a Decay3Body pdf model by a function that depends on other variables." );

  // Consider the function parameters as own ones.
  const std::map< std::string, Parameter >& parMap = left.getParMap();
  right._parMap.insert( parMap.begin(), parMap.end() );

  // Append the function to the functions vector.
  right._funcs.push_back( left );

  // Recompute the norm, since the pdf shape has changed under this operation.
  right.cache();

  return right;
}


const std::map< std::string, double > Decay3Body::generate() const throw( PdfException )
{
  // Generate mSq12 and mSq13, and compute mSq23 from these.
  const double& min12 = std::pow( _ps.m1()      + _ps.m2(), 2 );
  const double& min13 = std::pow( _ps.m1()      + _ps.m3(), 2 );
  const double& max12 = std::pow( _ps.mMother() - _ps.m3(), 2 );
  const double& max13 = std::pow( _ps.mMother() - _ps.m2(), 2 );

  const std::string& mSq12name = getVar( 0 ).name();
  const std::string& mSq13name = getVar( 1 ).name();
  const std::string& mSq23name = getVar( 2 ).name();

  // Sum of squared invariant masses of all particles (mother and daughters).
  const double& mSqSum = _ps.mSqMother() + _ps.mSq1() + _ps.mSq2() + _ps.mSq3();

  // Generate uniform mSq12 and mSq13.
  double mSq12 = 0.0;
  double mSq13 = 0.0;
  double mSq23 = 0.0;

  double pdfVal  = 0.0;

  // Attempts to generate an event.
  int count = 10000;

  std::map< std::string, double > values;

  while ( count-- )
  {
    mSq12 = Random::flat( min12, max12 );
    mSq13 = Random::flat( min13, max13 );
    mSq23 = mSqSum - mSq12 - mSq13;

    values[ mSq12name ] = mSq12;
    values[ mSq13name ] = mSq13;
    values[ mSq23name ] = mSq23;

    pdfVal = this->evaluate( mSq12, mSq13, mSq23 );

    if ( pdfVal > _maxPdf )
      std::cout << "Problem: " << pdfVal << " " << mSq12 << " " << mSq13 << " " << mSqSum - mSq12 - mSq13 << std::endl;

    // Apply the accept-reject decision.
    if ( Random::flat( 0.0, _maxPdf ) < pdfVal )
      return values;
  }

  values[ mSq12name ] = 0.0;
  values[ mSq13name ] = 0.0;
  values[ mSq23name ] = 0.0;

  return values;
}

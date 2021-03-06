#include "AlgSlopeOne.h"
#include "ProgressBar.h"

using namespace std;


AlgSlopeOne::AlgSlopeOne( DataReader& dreader,
                          int userpos,
                          int itempos,
                          int ratingpos )
: RecSysAlgorithm< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >( dreader, userpos, itempos, ratingpos )
{
   size_t nitems = m_ratingMatrix.items();
   m_pDevMatrix = new DenseMatrix( nitems, nitems );
   m_pCardMatrix = new DenseMatrix( nitems, nitems );
}

AlgSlopeOne::~AlgSlopeOne()
{
   if( NULL != m_pDevMatrix )
   {
      delete m_pDevMatrix;
   }
   if( NULL != m_pCardMatrix )
   {
      delete m_pCardMatrix;
   }
}

int AlgSlopeOne::train( FlowControl& fcontrol, bool progress )
{
   size_t nusers = m_ratingMatrix.users();
   size_t nitems = m_ratingMatrix.items();

   ProgressBar pbar( nusers, progress );
   for( size_t u = 0 ; u < nusers ; ++u )
   {
      SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> > row = m_ratingMatrix.userVector( u );
      SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >::iterator ind1;
      SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >::iterator end = row.end();
      for( ind1 = row.begin() ; ind1 != end ; ++ind1 )
      {
         double ru1 = *ind1;
         SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >::iterator ind2;
         SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >::iterator end2 = row.end();
         for( ind2 = row.begin() ; ind2 != end2 ; ++ind2 )
         {
            if( ind1 != ind2 )
            {
               size_t i = ind1.index();
               size_t j = ind2.index();
               double ru2 = *ind2;
               double diff = ru1 - ru2;
               m_pDevMatrix->add( i, j, diff );
               m_pCardMatrix->add( i, j, 1 );
            }

            if( fcontrol.interrupt() )
            {
               return STOPPED;
            }
         }

      }

      pbar.update( u + 1 );
   }

   for( size_t i = 0 ; i < nitems ; ++i )
   {
      for( size_t j = 0 ; j < nitems ; ++j )
      {
         double card = m_pCardMatrix->get( i, j );
         if( card > 0 )
         {
            double sum = m_pDevMatrix->get( i, j );
            m_pDevMatrix->set( i, j, sum / card );
         }

         if( fcontrol.interrupt() )
         {
            return STOPPED;
         }
      }
   }

   return FINISHED;
}

double AlgSlopeOne::predict( string& userId, string& itemId )
{
   int userrow = m_ratingMatrix.row( userId );
   if( userrow < 0 )
   {
      std::ostringstream eMsg;
      eMsg << "user id '" << userId << "' was not included in training set";
      throw invalid_argument( eMsg.str() );
   }
   int itemcol = m_ratingMatrix.column( itemId );
   if( itemcol < 0 )
   {
      std::ostringstream eMsg;
      eMsg << "item id '" << itemId << "' was not included in training set";
      throw invalid_argument( eMsg.str() );
   }
   return predict( userrow, itemcol );
}

double AlgSlopeOne::predict( size_t userrow, size_t itemcol )
{
   double sumpred = 0;
   double sumcard = 0;
   if( m_ratingMatrix.items() > itemcol )
   {
      SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> > row = m_ratingMatrix.userVector( userrow );
      SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >::iterator ind;
      SparseRow< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> >::iterator end = row.end();
      for( ind = row.begin() ; ind != end ; ++ind )
      {
         size_t itempos = ind.index();
         if( itemcol != itempos )
         {
            double rating = *ind;
            double card = m_pCardMatrix->get( itemcol, itempos );
            if( card > 0 )
            {
               sumpred += ( m_pDevMatrix->get( itemcol, itempos ) + rating )*card;
               sumcard += card;
            }
         }
      }
   }

   double rate = 0;
   if( sumpred > 0 )
   {
      rate = sumpred / sumcard;
      if( rate > 5 )
      {
         rate = 5;
      }
      else if( rate < 1 )
      {
         rate = 1;
      }
   }
   else
   {
      rate = m_globalMean;
   }

   return rate;
}



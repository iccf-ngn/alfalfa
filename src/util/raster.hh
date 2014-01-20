#ifndef RASTER_HH
#define RASTER_HH

#include <memory>

#include "2d.hh"
#include "safe_array.hh"

class MotionVector;

/* For an array of pixels, context and separate construction not necessary */
template<>
template< typename... Targs >
TwoD< uint8_t >::TwoD( const unsigned int width, const unsigned int height, Targs&&... Fargs )
  : width_( width ), height_( height ), storage_( width * height, Fargs... )
{
  assert( width > 0 );
  assert( height > 0 );
}

template <class integer>
static inline uint8_t clamp255( const integer value )
{
  if ( value < 0 ) return 0;
  if ( value > 255 ) return 255;
  return value;
}

class Raster
{
public:
  template <unsigned int size>
  class Block
  {
  public:
    typedef TwoDSubRange< uint8_t, size, 1 > Row;
    typedef TwoDSubRange< uint8_t, 1, size > Column;

  private:
    TwoDSubRange< uint8_t, size, size > contents_;
    typename TwoD< Block >::Context context_;

    void dc_predict( void );
    void dc_predict_simple( void );
    void vertical_predict( void );
    void horizontal_predict( void );
    void true_motion_predict( void );
    void vertical_smoothed_predict( void );
    void horizontal_smoothed_predict( void );
    void left_down_predict( void );
    void right_down_predict( void );
    void vertical_right_predict( void );
    void vertical_left_predict( void );
    void horizontal_down_predict( void );
    void horizontal_up_predict( void );

    struct Predictors {
      static const Row & row127( void );
      static const Column & col129( void );

      const Row above_row;
      const Column left_column;
      const uint8_t & above_left;

      /* non-const so macroblock can adjust the rightmost subblocks */
      struct AboveRightBottomRowPredictor {
	Row above_right_bottom_row;
	const uint8_t * above_bottom_right_pixel;
	bool use_row;

	uint8_t above_right( const unsigned int column ) const;
      } above_right_bottom_row_predictor;

      uint8_t above( const int8_t column ) const;
      uint8_t left( const int8_t row ) const;
      uint8_t east( const int8_t num ) const;

      Predictors( const typename TwoD< Block >::Context & context );
    } predictors_;

    uint8_t above( const int column ) const { return predictors_.above( column ); }
    uint8_t left( const int column ) const { return predictors_.left( column ); }
    uint8_t east( const int column ) const { return predictors_.east( column ); }

  public:
    Block( const typename TwoD< Block >::Context & c, TwoD< uint8_t > & raster_component );

    uint8_t & at( const unsigned int column, const unsigned int row )
    { return contents_.at( column, row ); }

    const uint8_t & at( const unsigned int column, const unsigned int row ) const
    { return contents_.at( column, row ); }

    unsigned int stride( void ) const { return contents_.stride(); }

    const decltype(contents_) & contents( void ) const { return contents_; }
    decltype(contents_) & mutable_contents( void ) { return contents_; }
    const Predictors & predictors( void ) const { return predictors_; }

    template <class PredictionMode>
    void intra_predict( const PredictionMode mb_mode );

    void inter_predict( const MotionVector & mv, const TwoD< uint8_t > & reference );

    template <class ReferenceType>
    void safe_inter_predict( const MotionVector & mv, const ReferenceType & reference,
			     const int source_column, const int source_row );

    void unsafe_inter_predict( const MotionVector & mv, const TwoD< uint8_t > & reference,
			       const int source_column, const int source_row );

    void set_above_right_bottom_row_predictor( const typename Predictors::AboveRightBottomRowPredictor & replacement );

    const typename TwoD< Block >::Context & context( void ) const { return context_; }

    static constexpr unsigned int dimension { size };

    Block & operator=( const Block & other );
    bool operator==( const Block & other ) const;
    SafeArray< SafeArray< int16_t, size >, size > operator-( const Block & other ) const;
  };

  using Block4  = Block< 4 >;
  using Block8  = Block< 8 >;
  using Block16 = Block< 16 >;

  struct Macroblock
  {
    Block16 & Y;
    Block8 & U;
    Block8 & V;
    TwoDSubRange< Block4, 4, 4 > Y_sub;
    TwoDSubRange< Block4, 2, 2 > U_sub, V_sub;

    Macroblock( const TwoD< Macroblock >::Context & c, Raster & raster );

    Macroblock & operator=( const Macroblock & other );
    bool operator==( const Macroblock & other ) const;
  };

private:
  unsigned int display_width_, display_height_;
  unsigned int width_ { 16 * macroblock_dimension( display_width_ ) },
    height_ { 16 * macroblock_dimension( display_height_ ) };

  TwoD< uint8_t > Y_ { width_, height_ },
    U_ { width_ / 2, height_ / 2 },
    V_ { width_ / 2, height_ / 2 };

  TwoD< Block4 > Y_subblocks_ { width_ / 4, height_ / 4, Y_ },
    U_subblocks_ { width_ / 8, height_ / 8, U_ },
    V_subblocks_ { width_ / 8, height_ / 8, V_ };

  TwoD< Block16 > Y_bigblocks_ { width_ / 16, height_ / 16, Y_ };
  TwoD< Block8 >  U_bigblocks_ { width_ / 16, height_ / 16, U_ };
  TwoD< Block8 >  V_bigblocks_ { width_ / 16, height_ / 16, V_ };

  TwoD< Macroblock > macroblocks_ { width_ / 16, height_ / 16, *this };

public:
  Raster( const unsigned int display_width, const unsigned int display_height );

  TwoD< uint8_t > & Y( void ) { return Y_; }
  TwoD< uint8_t > & U( void ) { return U_; }
  TwoD< uint8_t > & V( void ) { return V_; }

  const TwoD< uint8_t > & Y( void ) const { return Y_; }
  const TwoD< uint8_t > & U( void ) const { return U_; }
  const TwoD< uint8_t > & V( void ) const { return V_; }

  Macroblock & macroblock( const unsigned int column, const unsigned int row )
  {
    return macroblocks_.at( column, row );
  }

  const Macroblock & macroblock( const unsigned int column, const unsigned int row ) const
  {
    return macroblocks_.at( column, row );
  }

  unsigned int width( void ) const { return width_; }
  unsigned int height( void ) const { return height_; }
  unsigned int display_width( void ) const { return display_width_; }
  unsigned int display_height( void ) const { return display_height_; }

  static unsigned int macroblock_dimension( const unsigned int num ) { return ( num + 15 ) / 16; }
};

class RasterHandle
{
private:
  std::shared_ptr< Raster > raster_;

public:
  RasterHandle( const unsigned int display_width, const unsigned int display_height )
    : raster_( std::make_shared<Raster>( display_width, display_height ) )
  {}

  RasterHandle( const std::shared_ptr< Raster > & other )
    : raster_( other )
  {}

  operator const Raster & () const { return *raster_; }
  operator Raster & () { return *raster_; }
};

#endif /* RASTER_HH */

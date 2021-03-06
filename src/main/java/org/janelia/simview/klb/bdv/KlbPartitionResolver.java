package org.janelia.simview.klb.bdv;

import net.imglib2.type.NativeType;
import net.imglib2.type.numeric.RealType;
import org.janelia.simview.klb.KLB;
import org.janelia.simview.klb.jni.KlbImageIO;
import org.janelia.simview.klb.jni.KlbRoi;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Defines a dataset consisting of one or multiple KLB files.
 * <p>
 * Interfaces with Fiji's Big Data Viewer and SPIM plugins through
 * KlbSpimDataAdapter, which generates a SpimData2 instance that can
 * be consumed by Fiji and/or saved to XML.
 * <p>
 * Returns metadata that is apparent from the file system, such as the
 * number of ViewSetups, time points and scales.
 * <p>
 * Retrieves basic image-related metadata (image dimensions, block
 * dimensions, sampling).
 * <p>
 * Configures provided KlbImageIO and KlbRoi instances to read a block
 * defined by time point, ViewSetup, level, ROI start and
 * ROI dimensions.
 * <p>
 * Uses a user-defined path name tag pattern.
 */
public class KlbPartitionResolver< T extends RealType< T > & NativeType< T > >
{
    protected final String[] viewSetupTemplates;
    protected final int[] angleIds, channelIds, illuminationIds;
    protected final String[] angleNames, channelNames, illuminationNames;
    protected String timeTag, timeMatch, timeFormat;
    protected String resLvlTag, resLvlMatch, resLvlFormat;
    private double[][] sampling = null;
    private int firstTimePoint = 0, lastTimePoint = 0;
    private int numResolutionLevels = 1;
    private final KLB klb = KLB.newInstance();

    /**
     * Constructs a KlbPartitionResolver from a file system path following
     * a name tag pattern.
     *
     * @param template absolute file system path to a data file (e.g. '/folder/Data1Time000Chn00.klb')
     * @param nameTags list of KlbMultiFileNameTag instances
     */
    public KlbPartitionResolver( final String template, final List< KlbMultiFileNameTag > nameTags )
    {
        // NameTags that are found in template file path
        final List< KlbMultiFileNameTag > foundTags = new ArrayList< KlbMultiFileNameTag >();

        // Format of KlbMultiFileNameTag in template file path, in same order as 'foundTags', eg. 'TM%06d'
        final List< String > formats = new ArrayList< String >();

        // Depth
        final List< Integer > depths = new ArrayList< Integer >();
        
        for ( final KlbMultiFileNameTag tag : nameTags ) {
            if ( tag.tag.trim().isEmpty() ) {
                continue;
            }
            final Pattern pattern = Pattern.compile( String.format( "%s\\d+", tag.tag ) );
            final Matcher matcher = pattern.matcher( template );
            
            //resolution level is parsed with priority - always use the resLvlTag specified in the table
            if ( tag.dimension == KlbMultiFileNameTag.Dimension.RESOLUTION_LEVEL && tag.last > 0) {
                resLvlTag = tag.tag;
                resLvlMatch = null;
                numResolutionLevels = tag.last + 1;
            } 
            if ( matcher.find() ) {
                final String match = template.substring( matcher.start(), matcher.end() );
                final String format = String.format( "%s%s%dd", tag.tag, "%0", match.length() - tag.tag.length() );
                final int depth = 1 + (tag.last - tag.first) / tag.stride;
                if ( tag.dimension == KlbMultiFileNameTag.Dimension.TIME ) {
                    timeTag = tag.tag;
                    timeMatch = match;
                    timeFormat = format;
                    firstTimePoint = tag.first;
                    lastTimePoint = tag.last;
                } else if ( tag.dimension == KlbMultiFileNameTag.Dimension.RESOLUTION_LEVEL ) {
                    resLvlTag = tag.tag;
                    resLvlMatch = match;
                    resLvlFormat = format;
                    numResolutionLevels = tag.last + 1;
                } else {
                    if ( depth > 1 ) {
                        foundTags.add( tag );
                        formats.add( format );
                        depths.add( depth );
                    }
                }
            }
        }

        int numViewSetups = 1;
        for ( final int depth : depths ) {
            numViewSetups *= depth;
        }

        viewSetupTemplates = new String[ numViewSetups ];
        angleIds = new int[ numViewSetups ];
        channelIds = new int[ numViewSetups ];
        illuminationIds = new int[ numViewSetups ];
        angleNames = new String[ numViewSetups ];
        channelNames = new String[ numViewSetups ];
        illuminationNames = new String[ numViewSetups ];

        for ( int setup = 0; setup < numViewSetups; ++setup ) {
            String fn = template;
            int angleId = 0, channelId = 0, illuminationId = 0;
            String angleName = "0", channelName = "0", illuminationName = "0";
            for ( int d = 0; d < foundTags.size(); ++d ) {
                final KlbMultiFileNameTag tag = foundTags.get( d );
                final int depth = depths.get( d );

                int depthHigherDims = 1;
                for ( int i = d + 1; i < foundTags.size(); ++i ) {
                    depthHigherDims *= depths.get( i );
                }

                int id = setup / depthHigherDims;
                while ( id >= depth ) {
                    id -= depth;
                }
                final int name = id * tag.stride;

                switch ( tag.dimension ) {
                    case ANGLE:
                        angleId = id;
                        angleName = "" + name;
                        break;
                    case CHANNEL:
                        channelId = id;
                        channelName = "" + name;
                        break;
                    case ILLUMINATION:
                        illuminationId = id;
                        illuminationName = "" + name;
                        break;
                }

                fn = fn.replaceAll( String.format( "%s\\d+", tag.tag ), String.format( formats.get( d ), name ) );
            }
            
            //remove RESLVL tag if specified file is higher res level
            if ( resLvlMatch != null )
            	fn = fn.replace( "."+resLvlMatch , "");
            
            viewSetupTemplates[ setup ] = fn;
            angleIds[ setup ] = angleId;
            channelIds[ setup ] = channelId;
            illuminationIds[ setup ] = illuminationId;
            angleNames[ setup ] = angleName;
            channelNames[ setup ] = channelName;
            illuminationNames[ setup ] = illuminationName;
        }
    }

    public KlbPartitionResolver( final String[] viewSetupTemplates, final String timeTag, final int firstTimePoint, final int lastTimePoint, final String resolutionLevelTag, final int numResolutionLevels )
    {
        this.viewSetupTemplates = viewSetupTemplates;
        this.timeTag = timeTag;
        this.firstTimePoint = firstTimePoint;
        this.lastTimePoint = lastTimePoint;
        this.resLvlTag = resolutionLevelTag;
        this.numResolutionLevels = numResolutionLevels;
        angleIds = channelIds = illuminationIds = null;
        angleNames = channelNames = illuminationNames = null;

        final Pattern pattern = Pattern.compile( String.format( "%s\\d+", this.timeTag ) );
        for ( final String template : viewSetupTemplates ) {
            final Matcher matcher = pattern.matcher( template );
            if ( matcher.find() ) {
                timeMatch = template.substring( matcher.start(), matcher.end() );
                timeFormat = String.format( "%s%s%dd", this.timeTag, "%0", timeMatch.length() - this.timeTag.length() );
                break;
            }
        }
    }

    public void specifySampling( final double[][] sampling )
    {
        this.sampling = sampling;
    }

    public int getNumViewSetups()
    {
        return viewSetupTemplates.length;
    }

    public String getViewSetupName( final int viewSetup )
    {
        return new File( viewSetupTemplates[ viewSetup ] ).getName().replace( ".klb", "" );
    }

    public T getViewSetupImageType( final int viewSetup )
    {
        final String fp = getFilePath( getFirstTimePoint(), viewSetup, 0 );
        try {
            return ( T ) klb.readHeader( fp ).dataType;
        } catch ( IOException e ) {
            e.printStackTrace();
        }
        return null;
    }

    public int getAngleId( final int viewSetup )
    {
        return angleIds[ viewSetup ];
    }

    public int getChannelId( final int viewSetup )
    {
        return channelIds[ viewSetup ];
    }

    public int getIlluminationId( final int viewSetup )
    {
        return illuminationIds[ viewSetup ];
    }

    public String getAngleName( final int viewSetup )
    {
        return angleNames[ viewSetup ];
    }

    public String getChannelName( final int viewSetup )
    {
        return channelNames[ viewSetup ];
    }

    public String getIlluminationName( final int viewSetup )
    {
        return illuminationNames[ viewSetup ];
    }

    public int getFirstTimePoint()
    {
        return firstTimePoint;
    }

    public int getLastTimePoint()
    {
        return lastTimePoint;
    }

    /**
     * Returns the number of available resolution levels for the
     * given ViewSetup (channel).
     * Should be 1 (not 0) if only full resolution, original images
     * are available.
     *
     * @param viewSetup ViewSetup (channel) index
     * @return number of resolution levels
     */
    public int getNumResolutionLevels( final int viewSetup )
    {
        return getMaxNumResolutionLevels();
    }

    /**
     * Returns the highest number of available resolution levels
     * across all ViewSetups (channelIds).
     *
     * @return highest number of resolution levels across all channelIds
     */
    public int getMaxNumResolutionLevels()
    {
        return numResolutionLevels;
    }

    /**
     * Writes the dimensions (xyz) of the image defined by ViewSetup index,
     * time point and level into out. Returns false in case of failure
     * (e.g. file not found).
     *
     * @param timePoint time point
     * @param viewSetup ViewSetup index,
     * @param level     resolution level
     * @param out       target
     * @return whether or not successful
     */
    public boolean getImageDimensions( final int timePoint, final int viewSetup, final int level, final long[] out )
    {
        final String filePath = getFilePath( timePoint, viewSetup, level );
        try {
            final long[] dims = klb.readHeader( filePath ).imageSize;
            out[ 0 ] = dims[ 0 ];
            out[ 1 ] = dims[ 1 ];
            out[ 2 ] = dims[ 2 ];
            return true;
        } catch ( IOException e ) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Writes the block dimensions (xyz) of the image defined by ViewSetup index,
     * time point and level into out. Returns false in case of failure
     * (e.g. file not found).
     *
     * @param timePoint time point
     * @param viewSetup ViewSetup index,
     * @param level     resolution level
     * @param out       target
     * @return whether or not successful
     */
    public boolean getBlockDimensions( final int timePoint, final int viewSetup, final int level, final int[] out )
    {
        final String filePath = getFilePath( timePoint, viewSetup, level );
        try {
            final long[] dims = klb.readHeader( filePath ).blockSize;
            out[ 0 ] = ( int ) dims[ 0 ];
            out[ 1 ] = ( int ) dims[ 1 ];
            out[ 2 ] = ( int ) dims[ 2 ];
            return true;
        } catch ( IOException e ) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Writes the spatial sampling ("voxel size") of the image defined by
     * ViewSetup index, time point and level into out. Returns false in
     * case of failure (e.g. file not found).
     *
     * @param timePoint time point
     * @param viewSetup ViewSetup index,
     * @param level     resolution level
     * @param out       target
     * @return whether or not successful
     */
    public boolean getSampling( final int timePoint, final int viewSetup, final int level, final double[] out )
    {
        if ( level == 0 && sampling != null ) {
            out[ 0 ] = sampling[ level ][ 0 ];
            out[ 1 ] = sampling[ level ][ 1 ];
            out[ 2 ] = sampling[ level ][ 2 ];
            return true;
        }
        final String filePath = getFilePath( timePoint, viewSetup, level );
        try {
            final float[] smpl = klb.readHeader( filePath ).pixelSpacing;
            out[ 0 ] = smpl[ 0 ];
            out[ 1 ] = smpl[ 1 ];
            out[ 2 ] = smpl[ 2 ];
            return true;
        } catch ( IOException e ) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Configures provided KlbImageIO and KlbRoi instances to read a
     * block defined by time point, ViewSetup index, resolution level,
     * block dimensions and block start.
     *
     * @param timePoint  time point index
     * @param viewSetup  ViewSetup index
     * @param level      resolution level
     * @param dimensions block dimensions
     * @param min        block start
     * @param io         reader
     * @param roi        ROI
     */
    @Deprecated
    public void set( final int timePoint, final int viewSetup, final int level, final int[] dimensions, final long[] min, final KlbImageIO io, final KlbRoi roi )
    {
        io.setFilename( getFilePath( timePoint, viewSetup, level ) );
        roi.setXyzctUB( new long[]{
                min[ 0 ] + dimensions[ 0 ] - 1,
                min[ 1 ] + dimensions[ 1 ] - 1,
                min[ 2 ] + dimensions[ 2 ] - 1,
                0, 0 } );
        roi.setXyzctLB( new long[]{ min[ 0 ], min[ 1 ], min[ 2 ], 0, 0 } );
    }

    protected String getFilePath( final int timePoint, final int viewSetup, final int level )
    {
        String fn = viewSetupTemplates[ viewSetup ];
        if ( timeMatch != null ) {
            fn = fn.replaceAll( timeMatch, String.format( timeFormat, timePoint ) );
        }

        if ( level == 0 && resLvlMatch != null ) {
            fn = fn.replace( "." + resLvlMatch, "" );
        } else if ( level > 0 ) {
            if ( resLvlMatch == null ) {
                fn = fn.substring( 0, fn.lastIndexOf( ".klb" ) ) + String.format( ".RESLVL%d.klb", level );
            } else {
                fn = fn.replaceAll( resLvlMatch, String.format( resLvlFormat, level ) );
            }
        }
        return fn;
    }
}

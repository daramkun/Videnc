using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Daramee.Videnc;
using Daramee.Videnc.Processors.Video;

namespace Daramee.Videnc.Test
{
	class Program
	{
		static void Main ( string [] args )
		{
			Console.CursorVisible = false;

			Stream inputStream, outputStream;
			inputStream = new FileStream ( @"E:\Downloads\Ed Sheeran - Castle On The Hill ( cover by J.Fla ).mp4", FileMode.Open );
			outputStream = new FileStream ( @"Z:\Test.mp4", FileMode.Create );
			using ( MPEG4Streamer streamer = new MPEG4Streamer ( outputStream, new MPEG4StreamSettings ()
			{
				HardwareAccelerationEncoding = true,

				VideoBitrate = ( uint ) VideoBitrate._12Mbps,
				AudioBitrate = AudioBitrate._192Kbps

			}, inputStream ) )
			{
				streamer.Processor = new Processor ()
				{
					Items = new List<ProcessorItem> ()
					{
						new ProcessorItem ()
						{
							Target = ProcessorTarget.Video,
							Processor = "blur",
							Timeline = new List<ProcessorTimeline> ()
							{
								new ProcessorTimeline ()
								{
									Time = new TimeSpan ( 0, 0, 0, 0, 0 ),
									Option = new Dictionary<string, object> ()
									{
										{ "range", new RectangleF ( 0, 0, 1920, 1080 ) },
										{ "deviation", 3.0f }
									}
								},
								new ProcessorTimeline ()
								{
									Time = new TimeSpan ( 0, 0, 2, 58, 0 ),
									Option = new Dictionary<string, object> ()
									{
										{ "range", new RectangleF ( 0, 0, 1920, 1080 ) },
										{ "deviation", 3.0f }
									}
								}
							}
						}
					}
				};
				streamer.Processor.AddVideoProcessor ( new SharpenFilterProcessor () );
				streamer.Processor.AddVideoProcessor ( new GaussianBlurFilterProcessor () );
				streamer.Processor.AddVideoProcessor ( new GrayscaleFilterProcessor () );
				streamer.Processor.AddVideoProcessor ( new InvertFilterProcessor () );
				streamer.Processor.AddVideoProcessor ( new SepiaFilterProcessor () );
				streamer.Processor.AddVideoProcessor ( new AddImageProcessor () );

				Stopwatch elapsed = new Stopwatch ();
				elapsed.Start ();

				streamer.StartStreaming ();
				while ( streamer.IsStreamingStarted )
				{
					Console.WriteLine ( "Percentage: {0}", streamer.Proceed * 100 );
					Console.WriteLine ( "Elapsed Time: {0}", elapsed.Elapsed );
					Console.WriteLine ( "Remaining Time: {0}", Utilities.CalculateRemainingTime ( elapsed.Elapsed, streamer.Proceed ) );
					Console.CursorLeft = 0;
					Console.CursorTop = 0;
				}
				streamer.StopStreaming ();

				elapsed.Stop ();

				Direct2DUtility.Cleanup ();
			}
			outputStream.Dispose ();
			inputStream.Dispose ();
		}
	}
}

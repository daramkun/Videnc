using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace Daramee.Videnc
{
	public static class Utilities
	{
		#region ProcessorItem Utility Methods
		public static IEnumerable<ProcessorItem> GetProcessorItemsFromStream ( Stream stream )
		{
			List<ProcessorItem> ret = new List<ProcessorItem> ();
			using ( TextReader textReader = new StreamReader ( stream, true ) )
			{
				using ( JsonTextReader reader = new JsonTextReader ( textReader ) )
				{
					JsonSerializer serializer = new JsonSerializer ();
					var obj = serializer.Deserialize<JObject> ( reader );
					
					var filters = obj [ "filters" ] as JObject;
					if ( filters == null )
						return null;

					if ( filters [ "video" ] is JArray video )
						AddFilter ( ret, video, ProcessorTarget.Video );

					if ( filters [ "audio" ] is JArray audio )
						AddFilter ( ret, audio, ProcessorTarget.Audio );
				}
			}
			return ret;
		}
		#endregion

		#region Time Utility Methods
		public static TimeSpan CalculateRemainingTime ( TimeSpan timeSpan, double proceedRatio )
		{
			return proceedRatio != 0 ? TimeSpan.FromSeconds ( ( timeSpan.TotalSeconds / proceedRatio ) * ( 1 - proceedRatio ) ) : TimeSpan.FromDays ( 999 );
		}

		public static bool GetOptimalTimelineOptions ( TimeSpan timeSpan, ProcessorItem item, out ProcessorTimeline start, out ProcessorTimeline end )
		{
			for ( int i = 0; i < item.Timeline.Count; ++i )
			{
				ProcessorTimeline current = item.Timeline [ i ];
				ProcessorTimeline next = item.Timeline [ i + 1 ];

				if ( timeSpan >= current.Time && timeSpan <= next.Time )
				{
					start = current;
					end = next;
					return true;
				}
			}

			start = null;
			end = null;

			return false;
		}
		#endregion

		#region Weight Value Calculate Utility Methods
		public static int WeightedInteger ( TimeSpan start, TimeSpan end, TimeSpan current, int startValue, int endValue )
		{
			double weight = ( end - current ).TotalMilliseconds / ( end - start ).TotalMilliseconds;
			return ( int ) ( endValue - ( ( endValue - startValue ) * weight ) );
		}

		public static float WeightedFloat ( TimeSpan start, TimeSpan end, TimeSpan current, float startValue, float endValue )
		{
			double weight = ( end - current ).TotalMilliseconds / ( end - start ).TotalMilliseconds;
			return ( float ) ( endValue - ( ( endValue - startValue ) * weight ) );
		}

		public static PointF WeightedPoint ( TimeSpan start, TimeSpan end, TimeSpan current, PointF startValue, PointF endValue )
		{
			return new PointF (
				WeightedFloat ( start, end, current, startValue.X, endValue.X ),
				WeightedFloat ( start, end, current, startValue.Y, endValue.Y )
				);
		}

		public static RectangleF WeightedRectangle ( TimeSpan start, TimeSpan end, TimeSpan current, RectangleF startValue, RectangleF endValue )
		{
			return new RectangleF (
				WeightedFloat ( start, end, current, startValue.X, endValue.X ),
				WeightedFloat ( start, end, current, startValue.Y, endValue.Y ),
				WeightedFloat ( start, end, current, startValue.Width, endValue.Width ),
				WeightedFloat ( start, end, current, startValue.Height, endValue.Height )
				);
		}
		#endregion

		#region Private Static Methods
		private static void AddFilter ( List<ProcessorItem> ret, JArray filterItem, ProcessorTarget target )
		{
			foreach ( JObject filter in filterItem )
			{
				ProcessorItem item = new ProcessorItem ()
				{
					Target = target,
					Processor = filter [ "type" ].Value<string> ()
				};
				foreach ( JObject timeline in filter [ "timeline" ] as JArray )
				{
					ProcessorTimeline ptl = new ProcessorTimeline ()
					{
						Time = TimeSpan.Parse ( timeline [ "time" ].Value<string> () )
					};
					foreach ( var option in timeline [ "option" ] as JObject )
					{
						object value;
						switch ( option.Value.Type )
						{
							case JTokenType.Null: value = null; break;
							case JTokenType.Integer: value = option.Value.Value<int> (); break;
							case JTokenType.Float: value = option.Value.Value<float> (); break;
							case JTokenType.Boolean: value = option.Value.Value<bool> (); break;
							case JTokenType.Date: value = option.Value.Value<DateTime> (); break;
							case JTokenType.TimeSpan: value = option.Value.Value<TimeSpan> (); break;
							case JTokenType.Guid: value = option.Value.Value<Guid> (); break;
							case JTokenType.String:
								{
									string str = option.Value.Value<string> ();
									if ( RectangleRegex.IsMatch ( str ) )
									{
										var match = RectangleRegex.Match ( str );
										RectangleF rect = new RectangleF ()
										{
											X = float.Parse ( match.Groups [ 1 ].Value ),
											Y = float.Parse ( match.Groups [ 3 ].Value ),
											Width = float.Parse ( match.Groups [ 5 ].Value ),
											Height = float.Parse ( match.Groups [ 7 ].Value ),
										};
										value = rect;
									}
									else if ( PointRegex.IsMatch ( str ) )
									{
										var match = PointRegex.Match ( str );
										PointF point = new PointF ()
										{
											X = float.Parse ( match.Groups [ 1 ].Value ),
											Y = float.Parse ( match.Groups [ 3 ].Value ),
										};
										value = point;
									}
									else if ( TimeSpanRegex.IsMatch ( str ) )
									{
										var match = TimeSpanRegex.Match ( str );
										TimeSpan timeSpan = new TimeSpan ( 0,
											int.Parse ( match.Groups [ 1 ].Value ),
											int.Parse ( match.Groups [ 2 ].Value ),
											int.Parse ( match.Groups [ 3 ].Value ),
											match.Groups.Count > 4 ? int.Parse ( match.Groups [ 4 ].Value ) : 0 );
										value = timeSpan;
									}
									else value = str;
								}
								break;
							case JTokenType.Bytes:
								{
									value = option.Value.Value<byte []> ();
								}
								break;
							default: value = null; break;
						}
						ptl.Option.Add ( option.Key, value );
					}
				}
				ret.Add ( item );
			}
		}
		#endregion

		#region Private Static Fields
		static readonly Regex TimeSpanRegex = new Regex ( "([0-9]+):([0-9][0-9]):([0-9][0-9])(.([0-9][0-9][0-9]))?" );
		static readonly Regex RectangleRegex = new Regex ( "(-?[0-9]+(.[0-9]+)?), ?(-?[0-9]+(.[0-9]+)?), ?(-?[0-9]+(.[0-9]+)?), ?(-?[0-9]+(.[0-9]+)?)" );
		static readonly Regex PointRegex = new Regex ( "(-?[0-9]+(.[0-9]+)?), ?(-?[0-9]+(.[0-9]+)?)" );
		#endregion
	}
}

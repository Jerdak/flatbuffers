#ifndef FLATBUFFERS_RPLY_H
#define FLATBUFFERS_RPLY_H

#include <flatbuffers\flatbuffers.h>
#include <flatbuffers\idl.h>
#include <flatbuffers\util.h>

#include <string>
#include <vector>
#include <rply.h>

namespace flatbuffers {
    namespace rply {
        namespace types {
            struct PlyVertex {
                double x,y,z;
                std::string ToString(const char delim=' '){
                    std::stringstream ss;
                    ss << x << delim << y << delim << z;
                    return ss.str();
                }
            };

            struct PlyFace {
                unsigned int inds[3];
                size_t size;
                std::string ToString(const char delim=' '){
                    std::stringstream ss;
                    ss << inds[0] << delim << inds[1] << delim << inds[2];
                    return ss.str();
                }
            };  

            struct PlyFlatBuffer {
                size_t size;
                char *data;
            };
        };

        class BasePly {
        public:
            BasePly(){}
            virtual ~BasePly(){
                Clear();
            }

            p_ply& ply(){return ply_;}
            const p_ply& ply() const { return ply_; }

            std::vector<flatbuffers::rply::types::PlyVertex>& vertices(){return vertices_;}
            const std::vector<flatbuffers::rply::types::PlyVertex>& vertices()const{return vertices_;}
            
            std::vector<flatbuffers::rply::types::PlyFace>& faces(){return faces_;}
            const std::vector<flatbuffers::rply::types::PlyFace>& faces()const{return faces_;}

            void set_vertices(const std::vector<flatbuffers::rply::types::PlyVertex>& vertices){
                vertices_ = vertices ;
            }

            void set_faces(const std::vector<flatbuffers::rply::types::PlyFace>& faces){
                faces_ = faces;
            }
            virtual void Clear(){
                vertices_.clear();
                faces_.clear();
            }
            virtual int Load(const std::string &file_name){
                Clear();

                long nvertices, ntriangles, nfbbs;
                ply_ = ply_open(file_name.c_str(), NULL, 0, NULL);
                if (!ply_) return 1;
                if (!ply_read_header(ply_)) return 1;

                register_callbacks();

                if (!ply_read(ply_)) return 1;
                ply_close(ply_);
            }
            static int VertexCallback(p_ply_argument argument) {
                BasePly *klass;

                long p;
                ply_get_argument_user_data(argument,(void**)&klass, &p);    //callback trampoline to original class

                switch(p){
                    case 0:
                        klass->vertices().push_back(flatbuffers::rply::types::PlyVertex());
                        klass->vertices()[klass->vertices().size()-1].x = ply_get_argument_value(argument);
                        break;
                    case 1:
                        klass->vertices()[klass->vertices().size()-1].y = ply_get_argument_value(argument);
                        break;
                    case 2:
                        klass->vertices()[klass->vertices().size()-1].z = ply_get_argument_value(argument);
                        break;
                    default:
                        printf("Unknown vertex index: %d\n",p);
                }
                return 1;
            }
            static int FaceCallback(p_ply_argument argument) {
                BasePly *klass;

                long length, value_index, dummy;
                ply_get_argument_property(argument, NULL, &length, &value_index);
                ply_get_argument_user_data(argument,(void**)&klass,&dummy);         //callback trampoline to original class
                switch (value_index) {
                    case 0:
                        klass->faces().push_back(flatbuffers::rply::types::PlyFace());
                        klass->faces()[klass->faces().size()-1].inds[0] = ply_get_argument_value(argument);
                        break;
                    case 1: 
                        klass->faces()[klass->faces().size()-1].inds[1] = ply_get_argument_value(argument);
                        break;
                    case 2:
                        klass->faces()[klass->faces().size()-1].inds[2] = ply_get_argument_value(argument);
                        break;
                    default: 
                        break;
                }
                return 1;
            }
        protected:
            virtual void register_callbacks(){
                ply_set_read_cb(ply_, "vertex", "x", &BasePly::VertexCallback, this, 0);
                ply_set_read_cb(ply_, "vertex", "y", &BasePly::VertexCallback, this, 1);
                ply_set_read_cb(ply_, "vertex", "z", &BasePly::VertexCallback, this, 2);
                ply_set_read_cb(ply_, "face", "vertex_indices", &BasePly::FaceCallback, this, 0);
            }
        protected:
            std::vector<flatbuffers::rply::types::PlyVertex> vertices_;
            std::vector<flatbuffers::rply::types::PlyFace> faces_;
            p_ply ply_;
        };

        class FaceBufferPly : public BasePly {
        public:
            FaceBufferPly(){}
            virtual ~FaceBufferPly(){
                Clear();
            }

            std::vector<flatbuffers::rply::types::PlyFlatBuffer>& flatbuffers(){return flatbuffers_;}
            const std::vector<flatbuffers::rply::types::PlyFlatBuffer>& flatbuffers()const{return flatbuffers_;}

            // Return raw data pointer to *first* flatbuffer element only (only 1 fbb should be stored per ply)
            char *byte_data(){
                return (flatbuffers_.size()==0)?NULL:flatbuffers_[0].data;
            }

            // Return raw data pointer to *first* flatbuffer element only (only 1 fbb should be stored per ply)
            size_t byte_length(){
                return (flatbuffers_.size()==0)?0:flatbuffers_[0].size;
            }

            static int FaceCallback(p_ply_argument argument) {
                FaceBufferPly *klass;
                long length, value_index, dummy;

                ply_get_argument_property(argument, NULL, &length, &value_index);
                ply_get_argument_user_data(argument,(void**)&klass,&dummy);         //callback trampoline to original class
                
                switch(value_index){
                    case -1:
                        klass->flatbuffers().push_back(flatbuffers::rply::types::PlyFlatBuffer());
                        klass->flatbuffers()[klass->flatbuffers().size()-1].size = length;
                        klass->flatbuffers()[klass->flatbuffers().size()-1].data = new char[length];
                        break;
                    default:
                        if(klass->flatbuffers()[klass->flatbuffers().size()-1].size==0){
                            printf("fbb[%d] data size unset or zero, invalid data\n",klass->flatbuffers().size()-1);
                            return 1;
                        }
                        klass->flatbuffers()[klass->flatbuffers().size()-1].data[value_index] = char(ply_get_argument_value(argument));
                }
                
                return 1;
            }
            virtual void Clear(){
                BasePly::Clear();
                flatbuffers_.clear();
            }
            virtual int Load(const std::string& file_name){
                return BasePly::Load(file_name);
            }
        protected:
            virtual void register_callbacks(){
                BasePly::register_callbacks();
                ply_set_read_cb(ply_, "fbb_example", "buffer", &FaceBufferPly::FaceCallback, this, 0);
            }
            std::vector<flatbuffers::rply::types::PlyFlatBuffer> flatbuffers_;
        };
       
        namespace utilities{
            inline int FlatBufferFactory(const std::string& file_name, flatbuffers::FlatBufferBuilder& fbb){
                flatbuffers::rply::FaceBufferPly ply;
                ply.Load(file_name);
                fbb.PushBytes((uint8_t*)(ply.byte_data()), ply.byte_length());

                return 1;
            }
        }   //end flatbuffers::rply::utilities
    }   //end flatbuffers::rply
} //end flatbuffers





#endif //FLATBUFFERS_RPLY_H